#include "hybrid_fusion.hpp"
#include <cassert>
#include <iostream>

using namespace brain_ai;

void test_hybrid_fusion() {
    // Test basic fusion
    {
        FusionWeights weights;
        weights.vector_weight = 0.6f;
        weights.episodic_weight = 0.2f;
        weights.semantic_weight = 0.2f;
        
        HybridFusion fusion(weights);
        
        std::vector<ScoredResult> vector_results = {
            ScoredResult("result1", 0.9f, "vector")
        };
        
        std::vector<ScoredResult> episodic_results = {
            ScoredResult("result2", 0.8f, "episodic")
        };
        
        std::vector<ScoredResult> semantic_results = {
            ScoredResult("result3", 0.7f, "semantic")
        };
        
        auto fused = fusion.fuse(vector_results, episodic_results, semantic_results, 10);
        
        assert(!fused.empty() && "Should have fused results");
        assert(fused.size() == 3 && "Should have 3 unique results");
    }
    
    // Test weight normalization
    {
        FusionWeights weights;
        weights.vector_weight = 1.0f;
        weights.episodic_weight = 1.0f;
        weights.semantic_weight = 1.0f;
        weights.normalize();
        
        float sum = weights.vector_weight + weights.episodic_weight + weights.semantic_weight;
        assert(std::abs(sum - 1.0f) < 0.001f && "Weights should sum to 1.0");
    }
    
    // Test deduplication (same content from multiple sources)
    {
        HybridFusion fusion;
        
        std::vector<ScoredResult> vector_results = {
            ScoredResult("duplicate", 0.9f, "vector")
        };
        
        std::vector<ScoredResult> episodic_results = {
            ScoredResult("duplicate", 0.8f, "episodic")
        };
        
        std::vector<ScoredResult> semantic_results;
        
        auto fused = fusion.fuse(vector_results, episodic_results, semantic_results, 10);
        
        assert(fused.size() == 1 && "Should deduplicate same content");
        assert(fused[0].content == "duplicate" && "Should keep the content");
    }
    
    // Test set/get weights
    {
        HybridFusion fusion;
        
        FusionWeights new_weights;
        new_weights.vector_weight = 0.5f;
        new_weights.episodic_weight = 0.3f;
        new_weights.semantic_weight = 0.2f;
        
        fusion.set_weights(new_weights);
        auto retrieved = fusion.get_weights();
        
        assert(std::abs(retrieved.vector_weight - 0.5f) < 0.001f && "Weight should be set");
    }
    
    std::cout << "All hybrid fusion tests passed!\n";
}
