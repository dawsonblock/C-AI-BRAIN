#include "hybrid_fusion.hpp"
#include <algorithm>
#include <unordered_map>

namespace brain_ai {

HybridFusion::HybridFusion(const FusionWeights& weights)
    : weights_(weights) {
    weights_.normalize();
}

std::vector<ScoredResult> HybridFusion::fuse(
    const std::vector<ScoredResult>& vector_results,
    const std::vector<ScoredResult>& episodic_results,
    const std::vector<ScoredResult>& semantic_results,
    size_t top_k
) {
    // Build map of content → source scores
    std::unordered_map<std::string, std::unordered_map<std::string, float>> score_map;
    
    // Add vector scores
    for (const auto& result : vector_results) {
        score_map[result.content]["vector"] = result.score;
    }
    
    // Add episodic scores
    for (const auto& result : episodic_results) {
        score_map[result.content]["episodic"] = result.score;
    }
    
    // Add semantic scores
    for (const auto& result : semantic_results) {
        score_map[result.content]["semantic"] = result.score;
    }
    
    // Compute fused scores
    std::vector<ScoredResult> fused_results;
    fused_results.reserve(score_map.size());
    
    for (const auto& [content, scores] : score_map) {
        float vector_score = 0.0f;
        float episodic_score = 0.0f;
        float semantic_score = 0.0f;
        
        if (scores.find("vector") != scores.end()) {
            vector_score = scores.at("vector");
        }
        if (scores.find("episodic") != scores.end()) {
            episodic_score = scores.at("episodic");
        }
        if (scores.find("semantic") != scores.end()) {
            semantic_score = scores.at("semantic");
        }
        
        float fused_score = compute_fused_score(vector_score, episodic_score, semantic_score);
        
        ScoredResult result(content, fused_score, "fused");
        result.metadata["vector_score"] = vector_score;
        result.metadata["episodic_score"] = episodic_score;
        result.metadata["semantic_score"] = semantic_score;
        
        fused_results.push_back(result);
    }
    
    // Sort by fused score (descending)
    std::sort(fused_results.begin(), fused_results.end(),
        [](const ScoredResult& a, const ScoredResult& b) {
            return a.score > b.score;
        });
    
    // Take top-k
    if (fused_results.size() > top_k) {
        fused_results.resize(top_k);
    }
    
    return fused_results;
}

void HybridFusion::set_weights(const FusionWeights& weights) {
    weights_ = weights;
    weights_.normalize();
}

void HybridFusion::learn_weights(
    const std::vector<ScoredResult>& results,
    const std::vector<float>& feedback_scores
) {
    // Simplified weight learning: Adjust based on correlation with feedback
    // Real implementation would use gradient descent or similar
    
    if (results.size() != feedback_scores.size() || results.empty()) {
        return;
    }
    
    // Compute correlation between each source and feedback
    float vector_corr = 0.0f;
    float episodic_corr = 0.0f;
    float semantic_corr = 0.0f;
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        float feedback = feedback_scores[i];
        
        if (result.metadata.find("vector_score") != result.metadata.end()) {
            vector_corr += result.metadata.at("vector_score") * feedback;
        }
        if (result.metadata.find("episodic_score") != result.metadata.end()) {
            episodic_corr += result.metadata.at("episodic_score") * feedback;
        }
        if (result.metadata.find("semantic_score") != result.metadata.end()) {
            semantic_corr += result.metadata.at("semantic_score") * feedback;
        }
    }
    
    // Update weights (simple proportional adjustment)
    float learning_rate = 0.1f;
    weights_.vector_weight += learning_rate * vector_corr / results.size();
    weights_.episodic_weight += learning_rate * episodic_corr / results.size();
    weights_.semantic_weight += learning_rate * semantic_corr / results.size();
    
    // Normalize
    weights_.normalize();
}

float HybridFusion::compute_fused_score(
    float vector_score,
    float episodic_score,
    float semantic_score
) const {
    return weights_.vector_weight * vector_score +
           weights_.episodic_weight * episodic_score +
           weights_.semantic_weight * semantic_score;
}

std::vector<ScoredResult> HybridFusion::merge_results(
    const std::vector<ScoredResult>& all_results
) {
    // Deduplicate by content, keeping highest score
    std::unordered_map<std::string, ScoredResult> unique_results;
    
    for (const auto& result : all_results) {
        auto it = unique_results.find(result.content);
        if (it == unique_results.end() || it->second.score < result.score) {
            unique_results[result.content] = result;
        }
    }
    
    // Convert to vector
    std::vector<ScoredResult> merged;
    merged.reserve(unique_results.size());
    
    for (const auto& [content, result] : unique_results) {
        merged.push_back(result);
    }
    
    return merged;
}

} // namespace brain_ai
