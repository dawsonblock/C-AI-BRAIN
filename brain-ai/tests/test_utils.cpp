#include "utils.hpp"
#include <iostream>
#include <cassert>

using namespace brain_ai;

void test_utils() {
    // Test cosine similarity
    {
        std::vector<float> a = {1.0f, 0.0f, 0.0f};
        std::vector<float> b = {1.0f, 0.0f, 0.0f};
        float sim = cosine_similarity(a, b);
        assert(std::abs(sim - 1.0f) < 0.001f && "Identical vectors should have similarity 1.0");
    }
    
    {
        std::vector<float> a = {1.0f, 0.0f, 0.0f};
        std::vector<float> b = {0.0f, 1.0f, 0.0f};
        float sim = cosine_similarity(a, b);
        assert(std::abs(sim) < 0.001f && "Orthogonal vectors should have similarity 0.0");
    }
    
    // Test L2 distance
    {
        std::vector<float> a = {0.0f, 0.0f, 0.0f};
        std::vector<float> b = {1.0f, 0.0f, 0.0f};
        float dist = l2_distance(a, b);
        assert(std::abs(dist - 1.0f) < 0.001f && "Distance should be 1.0");
    }
    
    // Test vector normalization
    {
        std::vector<float> v = {3.0f, 4.0f};
        auto normalized = normalize_vector(v);
        float norm = std::sqrt(normalized[0] * normalized[0] + 
                               normalized[1] * normalized[1]);
        assert(std::abs(norm - 1.0f) < 0.001f && "Normalized vector should have unit length");
    }
    
    // Test sigmoid
    {
        assert(std::abs(sigmoid(0.0f) - 0.5f) < 0.001f && "Sigmoid of 0 should be 0.5");
        assert(sigmoid(100.0f) > 0.99f && "Sigmoid of large positive should be ~1.0");
        assert(sigmoid(-100.0f) < 0.01f && "Sigmoid of large negative should be ~0.0");
    }
    
    // Test softmax
    {
        std::vector<float> logits = {1.0f, 2.0f, 3.0f};
        auto probs = softmax(logits);
        float sum = 0.0f;
        for (float p : probs) sum += p;
        assert(std::abs(sum - 1.0f) < 0.001f && "Softmax should sum to 1.0");
        assert(probs[2] > probs[1] && probs[1] > probs[0] && "Softmax should preserve order");
    }
    
    // Test tokenization
    {
        std::string text = "hello world test";
        auto tokens = tokenize(text, ' ');
        assert(tokens.size() == 3 && "Should have 3 tokens");
        assert(tokens[0] == "hello" && "First token should be 'hello'");
        assert(tokens[2] == "test" && "Third token should be 'test'");
    }
    
    // Test lowercase conversion
    {
        std::string text = "Hello WORLD";
        std::string lower = to_lowercase(text);
        assert(lower == "hello world" && "Should be all lowercase");
    }
    
    std::cout << "All utils tests passed!\n";
}
