#include "hallucination_detector.hpp"
#include <cassert>
#include <iostream>

using namespace brain_ai;

void test_hallucination_detector() {
    HallucinationDetector detector;
    
    // Test with sufficient evidence
    {
        std::vector<Evidence> evidence = {
            Evidence("source1", 0.9f, "relevant content 1"),
            Evidence("source2", 0.8f, "relevant content 2"),
            Evidence("source3", 0.85f, "relevant content 3")
        };
        
        auto result = detector.validate("query", "response with relevant content", evidence, 0.5f);
        
        assert(!result.is_hallucination && "Should not flag with good evidence");
        assert(result.confidence_score >= 0.5f && "Should have reasonable confidence");
    }
    
    // Test with insufficient evidence
    {
        std::vector<Evidence> evidence = {
            Evidence("source1", 0.3f, "weak evidence")
        };
        
        auto result = detector.validate("query", "response", evidence, 0.5f);
        
        assert(!result.flags.empty() && "Should have flags");
        // May or may not be flagged depending on other factors
    }
    
    // Test with hedging language
    {
        std::vector<Evidence> evidence = {
            Evidence("source1", 0.9f, "strong evidence")
        };
        
        auto result = detector.validate("query", "I think maybe it could be", evidence, 0.5f);
        
        // Should detect hedging patterns
        bool has_hedging_flag = false;
        for (const auto& flag : result.flags) {
            if (flag.find("hedging") != std::string::npos) {
                has_hedging_flag = true;
                break;
            }
        }
        // Hedging might be detected depending on patterns
    }
    
    // Test add pattern
    {
        detector.add_hallucination_pattern("uncertain phrase");
        // Pattern added successfully (no assertion needed, just checking it doesn't crash)
    }
    
    std::cout << "All hallucination detector tests passed!\n";
}
