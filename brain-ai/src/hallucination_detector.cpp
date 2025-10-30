#include "hallucination_detector.hpp"
#include "utils.hpp"
#include <algorithm>

namespace brain_ai {

HallucinationDetector::HallucinationDetector()
    : min_evidence_count_(2),
      min_evidence_confidence_(0.6f) {
    
    // Add common hallucination patterns
    hallucination_patterns_.insert("i think");
    hallucination_patterns_.insert("probably");
    hallucination_patterns_.insert("maybe");
    hallucination_patterns_.insert("possibly");
    hallucination_patterns_.insert("i'm not sure");
    hallucination_patterns_.insert("i believe");
    hallucination_patterns_.insert("it seems");
}

HallucinationResult HallucinationDetector::validate(
    const std::string& query,
    const std::string& response,
    const std::vector<Evidence>& evidence,
    float confidence_threshold
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HallucinationResult result;
    result.supporting_evidence = evidence;
    
    // Filter high-confidence evidence
    std::vector<Evidence> strong_evidence;
    for (const auto& ev : evidence) {
        if (ev.confidence >= min_evidence_confidence_) {
            strong_evidence.push_back(ev);
        }
    }
    
    // Flag 1: Insufficient evidence count
    if (strong_evidence.size() < min_evidence_count_) {
        result.flags.push_back("Insufficient evidence count (" + 
                               std::to_string(strong_evidence.size()) + " < " +
                               std::to_string(min_evidence_count_) + ")");
    }
    
    // Flag 2: Contains hedging language
    if (contains_hedging(response)) {
        result.flags.push_back("Response contains hedging language");
    }
    
    // Flag 3: Unsubstantiated claims
    if (contains_unsubstantiated_claims(response, strong_evidence)) {
        result.flags.push_back("Response contains unsubstantiated claims");
    }
    
    // Compute evidence support score
    float evidence_score = compute_evidence_support(response, strong_evidence);
    
    // Combine scores
    float penalty = result.flags.size() * 0.2f;  // Each flag reduces confidence by 20%
    result.confidence_score = evidence_score - penalty;
    result.confidence_score = std::max(0.0f, std::min(1.0f, result.confidence_score));
    
    // Determine if hallucination
    result.is_hallucination = (result.confidence_score < confidence_threshold);
    
    return result;
}

void HallucinationDetector::add_hallucination_pattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(mutex_);
    hallucination_patterns_.insert(to_lowercase(pattern));
}

bool HallucinationDetector::contains_hedging(const std::string& response) const {
    std::string response_lower = to_lowercase(response);
    
    for (const auto& pattern : hallucination_patterns_) {
        if (response_lower.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool HallucinationDetector::contains_unsubstantiated_claims(
    const std::string& response,
    const std::vector<Evidence>& evidence
) const {
    // Simplified heuristic: Check if response contains factual-sounding claims
    // but has no supporting evidence
    
    if (evidence.empty()) {
        // Look for factual indicators without evidence
        std::vector<std::string> factual_indicators = {
            "according to",
            "research shows",
            "studies indicate",
            "it is known that",
            "the fact is"
        };
        
        std::string response_lower = to_lowercase(response);
        for (const auto& indicator : factual_indicators) {
            if (response_lower.find(indicator) != std::string::npos) {
                return true;
            }
        }
    }
    
    return false;
}

float HallucinationDetector::compute_evidence_support(
    const std::string& response,
    const std::vector<Evidence>& evidence
) const {
    if (evidence.empty()) {
        return 0.0f;
    }
    
    // Simple heuristic: Average confidence of evidence, weighted by content overlap
    float total_score = 0.0f;
    float total_weight = 0.0f;
    
    std::string response_lower = to_lowercase(response);
    
    for (const auto& ev : evidence) {
        // Check content overlap (simplified)
        std::string content_lower = to_lowercase(ev.content);
        
        // Count common words (very simplified)
        auto response_words = tokenize(response_lower);
        auto content_words = tokenize(content_lower);
        
        size_t common_words = 0;
        for (const auto& word : response_words) {
            if (word.length() > 3) {  // Skip short words
                for (const auto& ev_word : content_words) {
                    if (word == ev_word) {
                        common_words++;
                        break;
                    }
                }
            }
        }
        
        // Compute overlap ratio
        float overlap_ratio = 0.0f;
        if (!response_words.empty()) {
            overlap_ratio = static_cast<float>(common_words) / 
                           static_cast<float>(response_words.size());
        }
        
        // Weight by overlap and confidence
        float weight = overlap_ratio;
        total_score += ev.confidence * weight;
        total_weight += weight;
    }
    
    if (total_weight == 0.0f) {
        // No evidence overlap - use average confidence
        float avg_confidence = 0.0f;
        for (const auto& ev : evidence) {
            avg_confidence += ev.confidence;
        }
        return avg_confidence / evidence.size();
    }
    
    return total_score / total_weight;
}

} // namespace brain_ai
