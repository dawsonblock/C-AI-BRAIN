#ifndef BRAIN_AI_HYBRID_FUSION_HPP
#define BRAIN_AI_HYBRID_FUSION_HPP

#include <vector>
#include <string>
#include <unordered_map>

namespace brain_ai {

// Multi-source result with score
struct ScoredResult {
    std::string content;
    float score;
    std::string source;  // e.g., "vector", "episodic", "semantic"
    std::unordered_map<std::string, float> metadata;  // Additional scores
    
    ScoredResult(const std::string& c = "", float s = 0.0f, const std::string& src = "")
        : content(c), score(s), source(src) {}
};

// Fusion weights for different sources
struct FusionWeights {
    float vector_weight = 0.6f;      // Vector search
    float episodic_weight = 0.2f;    // Episodic buffer
    float semantic_weight = 0.2f;    // Semantic network
    
    // Validate and normalize
    void normalize() {
        float sum = vector_weight + episodic_weight + semantic_weight;
        if (sum > 0.0f) {
            vector_weight /= sum;
            episodic_weight /= sum;
            semantic_weight /= sum;
        }
    }
};

// Combine scores from multiple sources
class HybridFusion {
public:
    explicit HybridFusion(const FusionWeights& weights = FusionWeights());
    
    // Fuse results from multiple sources
    std::vector<ScoredResult> fuse(
        const std::vector<ScoredResult>& vector_results,
        const std::vector<ScoredResult>& episodic_results,
        const std::vector<ScoredResult>& semantic_results,
        size_t top_k = 10
    );
    
    // Update fusion weights
    void set_weights(const FusionWeights& weights);
    FusionWeights get_weights() const { return weights_; }
    
    // Learn weights from feedback (optional advanced feature)
    void learn_weights(
        const std::vector<ScoredResult>& results,
        const std::vector<float>& feedback_scores
    );
    
private:
    FusionWeights weights_;
    
    // Combine scores for a single result from multiple sources
    float compute_fused_score(
        float vector_score,
        float episodic_score,
        float semantic_score
    ) const;
    
    // Merge results by content (deduplicate)
    std::vector<ScoredResult> merge_results(
        const std::vector<ScoredResult>& all_results
    );
};

} // namespace brain_ai

#endif // BRAIN_AI_HYBRID_FUSION_HPP
