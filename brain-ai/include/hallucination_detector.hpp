#ifndef BRAIN_AI_HALLUCINATION_DETECTOR_HPP
#define BRAIN_AI_HALLUCINATION_DETECTOR_HPP

#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>

namespace brain_ai {

// Evidence for a claim
struct Evidence {
    std::string source;  // e.g., "vector_search", "semantic_network", "episodic_buffer"
    float confidence;
    std::string content;
    
    Evidence(const std::string& src, float conf, const std::string& cont)
        : source(src), confidence(conf), content(cont) {}
};

// Result of hallucination detection
struct HallucinationResult {
    bool is_hallucination;
    float confidence_score;  // 0.0 = certain hallucination, 1.0 = certain valid
    std::vector<std::string> flags;  // Reasons if flagged
    std::vector<Evidence> supporting_evidence;
    
    HallucinationResult(bool halluc = false, float conf = 1.0f)
        : is_hallucination(halluc), confidence_score(conf) {}
};

// Detect hallucinations via evidence validation
class HallucinationDetector {
public:
    HallucinationDetector();
    
    // Validate response against evidence
    HallucinationResult validate(
        const std::string& query,
        const std::string& response,
        const std::vector<Evidence>& evidence,
        float confidence_threshold = 0.5f
    );
    
    // Add known patterns of hallucinations (optional enhancement)
    void add_hallucination_pattern(const std::string& pattern);
    
    // Configure thresholds
    void set_min_evidence_count(size_t count) { min_evidence_count_ = count; }
    void set_min_evidence_confidence(float conf) { min_evidence_confidence_ = conf; }
    
private:
    size_t min_evidence_count_;
    float min_evidence_confidence_;
    std::unordered_set<std::string> hallucination_patterns_;
    mutable std::mutex mutex_;
    
    // Check if response contains hedging language
    bool contains_hedging(const std::string& response) const;
    
    // Check if response makes specific claims without evidence
    bool contains_unsubstantiated_claims(
        const std::string& response,
        const std::vector<Evidence>& evidence
    ) const;
    
    // Compute evidence support score
    float compute_evidence_support(
        const std::string& response,
        const std::vector<Evidence>& evidence
    ) const;
};

} // namespace brain_ai

#endif // BRAIN_AI_HALLUCINATION_DETECTOR_HPP
