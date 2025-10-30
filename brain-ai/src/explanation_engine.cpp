#include "explanation_engine.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace brain_ai {

Explanation ExplanationEngine::generate_explanation(
    const std::string& query,
    const std::string& response,
    const std::vector<ReasoningStep>& reasoning_trace
) {
    Explanation explanation(query, response);
    explanation.reasoning_trace = reasoning_trace;
    explanation.overall_confidence = compute_overall_confidence(reasoning_trace);
    explanation.summary = generate_summary(reasoning_trace);
    
    return explanation;
}

ReasoningStep ExplanationEngine::create_vector_search_step(
    size_t num_results,
    float avg_similarity,
    const std::vector<std::string>& top_results
) {
    ReasoningStep step("vector_search", "Vector similarity search");
    step.details["num_results"] = std::to_string(num_results);
    step.details["avg_similarity"] = std::to_string(avg_similarity);
    step.confidence = avg_similarity;
    
    if (!top_results.empty()) {
        std::string results_str;
        for (size_t i = 0; i < std::min(size_t(3), top_results.size()); ++i) {
            if (i > 0) results_str += "; ";
            results_str += top_results[i];
        }
        step.details["top_results"] = results_str;
    }
    
    return step;
}

ReasoningStep ExplanationEngine::create_episodic_step(
    size_t num_episodes,
    float avg_relevance,
    const std::vector<std::string>& relevant_episodes
) {
    ReasoningStep step("episodic_retrieval", "Retrieved conversation context");
    step.details["num_episodes"] = std::to_string(num_episodes);
    step.details["avg_relevance"] = std::to_string(avg_relevance);
    step.confidence = avg_relevance;
    
    if (!relevant_episodes.empty()) {
        std::string episodes_str;
        for (size_t i = 0; i < std::min(size_t(2), relevant_episodes.size()); ++i) {
            if (i > 0) episodes_str += "; ";
            episodes_str += relevant_episodes[i];
        }
        step.details["relevant_context"] = episodes_str;
    }
    
    return step;
}

ReasoningStep ExplanationEngine::create_semantic_step(
    size_t num_concepts,
    float activation_level,
    const std::vector<std::string>& activated_concepts
) {
    ReasoningStep step("semantic_activation", "Semantic concept spreading");
    step.details["num_concepts"] = std::to_string(num_concepts);
    step.details["activation_level"] = std::to_string(activation_level);
    step.confidence = activation_level;
    
    if (!activated_concepts.empty()) {
        std::string concepts_str;
        for (size_t i = 0; i < std::min(size_t(5), activated_concepts.size()); ++i) {
            if (i > 0) concepts_str += ", ";
            concepts_str += activated_concepts[i];
        }
        step.details["activated_concepts"] = concepts_str;
    }
    
    return step;
}

ReasoningStep ExplanationEngine::create_fusion_step(
    float vector_weight,
    float episodic_weight,
    float semantic_weight,
    float final_score
) {
    ReasoningStep step("hybrid_fusion", "Combined evidence from multiple sources");
    step.details["vector_weight"] = std::to_string(vector_weight);
    step.details["episodic_weight"] = std::to_string(episodic_weight);
    step.details["semantic_weight"] = std::to_string(semantic_weight);
    step.details["final_score"] = std::to_string(final_score);
    step.confidence = final_score;
    
    return step;
}

ReasoningStep ExplanationEngine::create_hallucination_check_step(
    bool passed,
    float confidence,
    const std::vector<std::string>& flags
) {
    ReasoningStep step("hallucination_check", 
                       passed ? "Response validated" : "Response flagged for review");
    step.details["passed"] = passed ? "true" : "false";
    step.details["confidence"] = std::to_string(confidence);
    step.confidence = passed ? confidence : (1.0f - confidence);
    
    if (!flags.empty()) {
        std::string flags_str;
        for (size_t i = 0; i < flags.size(); ++i) {
            if (i > 0) flags_str += "; ";
            flags_str += flags[i];
        }
        step.details["flags"] = flags_str;
    }
    
    return step;
}

std::string ExplanationEngine::format_explanation(const Explanation& explanation) const {
    std::ostringstream oss;
    
    oss << "=== Query Explanation ===\n\n";
    oss << "Query: " << explanation.query << "\n";
    oss << "Response: " << explanation.response << "\n\n";
    oss << "Overall Confidence: " << std::fixed << std::setprecision(2) 
        << (explanation.overall_confidence * 100.0f) << "%\n\n";
    
    oss << "Reasoning Process:\n";
    for (size_t i = 0; i < explanation.reasoning_trace.size(); ++i) {
        const auto& step = explanation.reasoning_trace[i];
        oss << (i + 1) << ". " << step.description 
            << " (confidence: " << std::fixed << std::setprecision(2) 
            << (step.confidence * 100.0f) << "%)\n";
        
        for (const auto& [key, value] : step.details) {
            oss << "   - " << key << ": " << value << "\n";
        }
        oss << "\n";
    }
    
    oss << "Summary: " << explanation.summary << "\n";
    
    return oss.str();
}

std::string ExplanationEngine::format_as_json(const Explanation& explanation) const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"query\": \"" << explanation.query << "\",\n";
    oss << "  \"response\": \"" << explanation.response << "\",\n";
    oss << "  \"overall_confidence\": " << explanation.overall_confidence << ",\n";
    oss << "  \"reasoning_trace\": [\n";
    
    for (size_t i = 0; i < explanation.reasoning_trace.size(); ++i) {
        const auto& step = explanation.reasoning_trace[i];
        oss << "    {\n";
        oss << "      \"step_type\": \"" << step.step_type << "\",\n";
        oss << "      \"description\": \"" << step.description << "\",\n";
        oss << "      \"confidence\": " << step.confidence << ",\n";
        oss << "      \"details\": {\n";
        
        size_t j = 0;
        for (const auto& [key, value] : step.details) {
            oss << "        \"" << key << "\": \"" << value << "\"";
            if (++j < step.details.size()) oss << ",";
            oss << "\n";
        }
        
        oss << "      }\n";
        oss << "    }";
        if (i < explanation.reasoning_trace.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "  ],\n";
    oss << "  \"summary\": \"" << explanation.summary << "\"\n";
    oss << "}\n";
    
    return oss.str();
}

std::string ExplanationEngine::generate_summary(const std::vector<ReasoningStep>& reasoning_trace) const {
    std::ostringstream oss;
    
    // Count source types
    size_t vector_count = 0;
    size_t episodic_count = 0;
    size_t semantic_count = 0;
    bool hallucination_checked = false;
    bool hallucination_passed = true;
    
    for (const auto& step : reasoning_trace) {
        if (step.step_type == "vector_search") vector_count++;
        else if (step.step_type == "episodic_retrieval") episodic_count++;
        else if (step.step_type == "semantic_activation") semantic_count++;
        else if (step.step_type == "hallucination_check") {
            hallucination_checked = true;
            if (step.details.find("passed") != step.details.end()) {
                hallucination_passed = (step.details.at("passed") == "true");
            }
        }
    }
    
    oss << "Response generated using ";
    std::vector<std::string> sources;
    if (vector_count > 0) sources.push_back("vector search");
    if (episodic_count > 0) sources.push_back("conversation context");
    if (semantic_count > 0) sources.push_back("semantic knowledge");
    
    for (size_t i = 0; i < sources.size(); ++i) {
        if (i > 0) {
            if (i == sources.size() - 1) oss << " and ";
            else oss << ", ";
        }
        oss << sources[i];
    }
    
    oss << ".";
    
    if (hallucination_checked) {
        if (hallucination_passed) {
            oss << " Response validated against evidence.";
        } else {
            oss << " Response flagged for potential hallucination.";
        }
    }
    
    return oss.str();
}

float ExplanationEngine::compute_overall_confidence(const std::vector<ReasoningStep>& reasoning_trace) const {
    if (reasoning_trace.empty()) {
        return 0.0f;
    }
    
    // Average confidence across all steps
    float total = 0.0f;
    for (const auto& step : reasoning_trace) {
        total += step.confidence;
    }
    
    return total / reasoning_trace.size();
}

} // namespace brain_ai
