#include "explanation_engine.hpp"
#include <cassert>
#include <iostream>

using namespace brain_ai;

void test_explanation_engine() {
    ExplanationEngine engine;
    
    // Test create reasoning steps
    {
        auto step = ExplanationEngine::create_vector_search_step(10, 0.85f, {"result1", "result2"});
        assert(step.step_type == "vector_search" && "Step type should be vector_search");
        assert(step.confidence == 0.85f && "Confidence should match");
        assert(!step.details.empty() && "Should have details");
    }
    
    {
        auto step = ExplanationEngine::create_episodic_step(5, 0.9f, {"episode1"});
        assert(step.step_type == "episodic_retrieval" && "Step type should be episodic_retrieval");
    }
    
    {
        auto step = ExplanationEngine::create_semantic_step(8, 0.7f, {"concept1", "concept2"});
        assert(step.step_type == "semantic_activation" && "Step type should be semantic_activation");
    }
    
    {
        auto step = ExplanationEngine::create_fusion_step(0.6f, 0.2f, 0.2f, 0.85f);
        assert(step.step_type == "hybrid_fusion" && "Step type should be hybrid_fusion");
    }
    
    {
        auto step = ExplanationEngine::create_hallucination_check_step(true, 0.9f, {});
        assert(step.step_type == "hallucination_check" && "Step type should be hallucination_check");
    }
    
    // Test generate explanation
    {
        std::vector<ReasoningStep> trace;
        trace.push_back(ExplanationEngine::create_vector_search_step(10, 0.85f, {}));
        trace.push_back(ExplanationEngine::create_fusion_step(0.6f, 0.2f, 0.2f, 0.8f));
        
        auto explanation = engine.generate_explanation("query", "response", trace);
        
        assert(explanation.query == "query" && "Query should match");
        assert(explanation.response == "response" && "Response should match");
        assert(explanation.reasoning_trace.size() == 2 && "Should have 2 steps");
        assert(!explanation.summary.empty() && "Should have summary");
    }
    
    // Test format explanation
    {
        std::vector<ReasoningStep> trace;
        trace.push_back(ExplanationEngine::create_vector_search_step(5, 0.9f, {}));
        
        auto explanation = engine.generate_explanation("test query", "test response", trace);
        auto formatted = engine.format_explanation(explanation);
        
        assert(!formatted.empty() && "Formatted explanation should not be empty");
        assert(formatted.find("test query") != std::string::npos && "Should contain query");
    }
    
    // Test format as JSON
    {
        std::vector<ReasoningStep> trace;
        trace.push_back(ExplanationEngine::create_vector_search_step(3, 0.8f, {}));
        
        auto explanation = engine.generate_explanation("query", "response", trace);
        auto json = engine.format_as_json(explanation);
        
        assert(!json.empty() && "JSON should not be empty");
        assert(json.find("\"query\"") != std::string::npos && "Should have query field");
        assert(json.find("\"response\"") != std::string::npos && "Should have response field");
    }
    
    std::cout << "All explanation engine tests passed!\n";
}
