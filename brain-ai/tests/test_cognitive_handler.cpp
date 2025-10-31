#include "cognitive_handler.hpp"
#include <cassert>
#include <iostream>

using namespace brain_ai;

void test_cognitive_handler() {
    // Test initialization
    {
        CognitiveHandler handler(64, FusionWeights(), 4);
        
        assert(handler.episodic_buffer_size() == 0 && "Buffer should be empty initially");
        assert(handler.semantic_network_size() == 0 && "Network should be empty initially");
    }
    
    // Test populate semantic network
    {
        CognitiveHandler handler(128, FusionWeights(), 2);
        
        std::vector<std::pair<std::string, std::vector<float>>> concepts = {
            {"concept1", {1.0f, 0.0f}},
            {"concept2", {0.0f, 1.0f}}
        };
        
        std::vector<std::tuple<std::string, std::string, float>> relations = {
            {"concept1", "concept2", 0.8f}
        };
        
        handler.populate_semantic_network(concepts, relations);
        
        assert(handler.semantic_network_size() == 2 && "Should have 2 concepts");
    }
    
    // Test process query
    {
        CognitiveHandler handler(128, FusionWeights(), 4);
        
        // Add some documents to the index first
        handler.index_document("doc1", {1.0f, 0.0f, 0.0f, 0.0f}, "Test document 1");
        handler.index_document("doc2", {0.0f, 1.0f, 0.0f, 0.0f}, "Test document 2");
        
        std::vector<float> query_emb = {1.0f, 0.0f, 0.0f, 0.0f};
        
        auto response = handler.process_query("test query", query_emb);
        
        assert(!response.response.empty() && "Should have response");
        assert(!response.results.empty() && "Should have results");
        assert(!response.explanation.reasoning_trace.empty() && "Should have reasoning trace");
    }
    
    // Test add episode
    {
        CognitiveHandler handler(128, FusionWeights(), 3);
        
        std::vector<float> emb = {1.0f, 0.0f, 0.0f};
        
        handler.add_episode("query1", "response1", emb);
        
        assert(handler.episodic_buffer_size() == 1 && "Should have 1 episode");
        
        handler.add_episode("query2", "response2", emb);
        
        assert(handler.episodic_buffer_size() == 2 && "Should have 2 episodes");
    }
    
    // Test query with episodic memory
    {
        CognitiveHandler handler(128, FusionWeights(), 4);
        
        std::vector<float> emb = {1.0f, 0.0f, 0.0f, 0.0f};
        
        // Add some documents to the index
        handler.index_document("doc1", emb, "Document content");
        
        // Add episode
        handler.add_episode("previous query", "previous response", emb);
        
        // Query should retrieve episode
        auto response = handler.process_query("similar query", emb);
        
        assert(!response.results.empty() && "Should have results");
        // Episodic results should be included in reasoning trace
    }
    
    // Test query configuration
    {
        CognitiveHandler handler(128, FusionWeights(), 4);
        
        std::vector<float> emb = {1.0f, 0.0f, 0.0f, 0.0f};
        
        // Add document to index
        handler.index_document("doc1", emb, "Test content");
        
        QueryConfig config;
        config.use_episodic = false;
        config.use_semantic = false;
        config.check_hallucination = false;
        config.generate_explanation = false;
        
        auto response = handler.process_query("test", emb, config);
        
        assert(!response.response.empty() && "Should still have response");
        // With all features disabled, should only use vector search
    }
    
    std::cout << "All cognitive handler tests passed!\n";
}
