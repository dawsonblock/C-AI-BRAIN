#ifndef BRAIN_AI_EXPLANATION_ENGINE_HPP
#define BRAIN_AI_EXPLANATION_ENGINE_HPP

#include <string>
#include <vector>
#include <unordered_map>

namespace brain_ai {

// Step in reasoning trace
struct ReasoningStep {
    std::string step_type;  // e.g., "vector_search", "episodic_retrieval", "semantic_activation"
    std::string description;
    std::unordered_map<std::string, std::string> details;
    float confidence;
    
    ReasoningStep(const std::string& type = "", const std::string& desc = "", float conf = 1.0f)
        : step_type(type), description(desc), confidence(conf) {}
};

// Complete explanation for a query
struct Explanation {
    std::string query;
    std::string response;
    std::vector<ReasoningStep> reasoning_trace;
    float overall_confidence;
    std::string summary;
    
    Explanation(const std::string& q = "", const std::string& r = "", float conf = 1.0f)
        : query(q), response(r), overall_confidence(conf) {}
};

// Generate human-readable explanations
class ExplanationEngine {
public:
    ExplanationEngine() = default;
    
    // Create explanation from reasoning trace
    Explanation generate_explanation(
        const std::string& query,
        const std::string& response,
        const std::vector<ReasoningStep>& reasoning_trace
    );
    
    // Add reasoning step
    static ReasoningStep create_vector_search_step(
        size_t num_results,
        float avg_similarity,
        const std::vector<std::string>& top_results
    );
    
    static ReasoningStep create_episodic_step(
        size_t num_episodes,
        float avg_relevance,
        const std::vector<std::string>& relevant_episodes
    );
    
    static ReasoningStep create_semantic_step(
        size_t num_concepts,
        float activation_level,
        const std::vector<std::string>& activated_concepts
    );
    
    static ReasoningStep create_fusion_step(
        float vector_weight,
        float episodic_weight,
        float semantic_weight,
        float final_score
    );
    
    static ReasoningStep create_hallucination_check_step(
        bool passed,
        float confidence,
        const std::vector<std::string>& flags
    );
    
    // Format explanation as human-readable text
    std::string format_explanation(const Explanation& explanation) const;
    
    // Format as JSON (for API responses)
    std::string format_as_json(const Explanation& explanation) const;
    
private:
    // Generate summary from reasoning trace
    std::string generate_summary(const std::vector<ReasoningStep>& reasoning_trace) const;
    
    // Compute overall confidence from steps
    float compute_overall_confidence(const std::vector<ReasoningStep>& reasoning_trace) const;
};

} // namespace brain_ai

#endif // BRAIN_AI_EXPLANATION_ENGINE_HPP
