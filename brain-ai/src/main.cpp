#include "cognitive_handler.hpp"
#include "utils.hpp"
#include <iostream>
#include <iomanip>
#include <random>

using namespace brain_ai;

// Generate random embedding for demo purposes
std::vector<float> generate_random_embedding(size_t dim = 128) {
    static std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    
    std::vector<float> embedding(dim);
    for (size_t i = 0; i < dim; ++i) {
        embedding[i] = dis(gen);
    }
    
    // Normalize
    return normalize_vector(embedding);
}

// Print a separator line
void print_separator() {
    std::cout << "\n" << std::string(80, '=') << "\n\n";
}

int main() {
    std::cout << "Brain-AI v4.0 - Production Cognitive Architecture Demo\n";
    print_separator();
    
    // Initialize cognitive handler
    std::cout << "Initializing cognitive architecture...\n";
    CognitiveHandler handler(128);  // 128 episode capacity
    
    // Populate semantic network with sample domain knowledge
    std::cout << "Populating semantic network with domain knowledge...\n";
    
    std::vector<std::pair<std::string, std::vector<float>>> concepts = {
        {"machine_learning", generate_random_embedding()},
        {"neural_networks", generate_random_embedding()},
        {"deep_learning", generate_random_embedding()},
        {"artificial_intelligence", generate_random_embedding()},
        {"data_science", generate_random_embedding()},
        {"computer_vision", generate_random_embedding()},
        {"natural_language_processing", generate_random_embedding()},
        {"reinforcement_learning", generate_random_embedding()},
        {"supervised_learning", generate_random_embedding()},
        {"unsupervised_learning", generate_random_embedding()}
    };
    
    std::vector<std::tuple<std::string, std::string, float>> relations = {
        {"machine_learning", "neural_networks", 0.9f},
        {"machine_learning", "deep_learning", 0.8f},
        {"neural_networks", "deep_learning", 0.95f},
        {"deep_learning", "computer_vision", 0.7f},
        {"deep_learning", "natural_language_processing", 0.7f},
        {"machine_learning", "supervised_learning", 0.8f},
        {"machine_learning", "unsupervised_learning", 0.8f},
        {"machine_learning", "reinforcement_learning", 0.8f},
        {"artificial_intelligence", "machine_learning", 0.9f},
        {"data_science", "machine_learning", 0.85f}
    };
    
    handler.populate_semantic_network(concepts, relations);
    
    std::cout << "✓ Semantic network initialized with " 
              << handler.semantic_network_size() << " concepts\n";
    
    print_separator();
    
    // Demo queries
    std::vector<std::string> demo_queries = {
        "What is deep learning?",
        "How does reinforcement learning work?",
        "Explain neural networks",
        "Tell me about computer vision applications"
    };
    
    std::cout << "Running demo queries through cognitive pipeline...\n";
    print_separator();
    
    for (size_t i = 0; i < demo_queries.size(); ++i) {
        const std::string& query = demo_queries[i];
        
        std::cout << "Query #" << (i + 1) << ": " << query << "\n\n";
        
        // Generate query embedding
        auto query_embedding = generate_random_embedding();
        
        // Process query
        auto response = handler.process_query(query, query_embedding);
        
        // Display results
        std::cout << "Response: " << response.response << "\n\n";
        std::cout << "Overall Confidence: " << std::fixed << std::setprecision(2) 
                  << (response.overall_confidence * 100.0f) << "%\n\n";
        
        std::cout << "Top Results:\n";
        for (size_t j = 0; j < std::min(size_t(3), response.results.size()); ++j) {
            const auto& result = response.results[j];
            std::cout << "  " << (j + 1) << ". [" << result.source << "] "
                      << "Score: " << std::fixed << std::setprecision(3) << result.score
                      << " - " << result.content.substr(0, 60) << "...\n";
        }
        std::cout << "\n";
        
        // Hallucination check
        if (response.hallucination_check.is_hallucination) {
            std::cout << "⚠️  Hallucination Warning: Response flagged for review\n";
            std::cout << "   Confidence: " << std::fixed << std::setprecision(2)
                      << (response.hallucination_check.confidence_score * 100.0f) << "%\n";
            if (!response.hallucination_check.flags.empty()) {
                std::cout << "   Flags:\n";
                for (const auto& flag : response.hallucination_check.flags) {
                    std::cout << "     - " << flag << "\n";
                }
            }
        } else {
            std::cout << "✓ Response validated (confidence: " 
                      << std::fixed << std::setprecision(2)
                      << (response.hallucination_check.confidence_score * 100.0f) << "%)\n";
        }
        std::cout << "\n";
        
        // Print explanation
        std::cout << "Reasoning Trace:\n";
        for (size_t j = 0; j < response.explanation.reasoning_trace.size(); ++j) {
            const auto& step = response.explanation.reasoning_trace[j];
            std::cout << "  " << (j + 1) << ". " << step.description 
                      << " (confidence: " << std::fixed << std::setprecision(2)
                      << (step.confidence * 100.0f) << "%)\n";
        }
        std::cout << "\n";
        
        std::cout << "Summary: " << response.explanation.summary << "\n";
        
        // Add episode to episodic buffer for next queries
        handler.add_episode(query, response.response, query_embedding);
        
        print_separator();
    }
    
    // Statistics
    std::cout << "System Statistics:\n";
    std::cout << "  Episodic Buffer: " << handler.episodic_buffer_size() << " episodes stored\n";
    std::cout << "  Semantic Network: " << handler.semantic_network_size() << " concepts\n";
    
    print_separator();
    
    // Test episodic memory retrieval
    std::cout << "Testing Episodic Memory Retrieval...\n\n";
    std::cout << "Query: \"Tell me again about deep learning\"\n\n";
    
    auto repeat_query_embedding = generate_random_embedding();
    auto repeat_response = handler.process_query(
        "Tell me again about deep learning",
        repeat_query_embedding
    );
    
    std::cout << "Response: " << repeat_response.response << "\n\n";
    std::cout << "Episodic Context Retrieved: " 
              << repeat_response.explanation.summary << "\n";
    
    print_separator();
    
    std::cout << "✓ Demo completed successfully!\n\n";
    std::cout << "Brain-AI v4.0 Features Demonstrated:\n";
    std::cout << "  ✓ Episodic memory (conversation context retention)\n";
    std::cout << "  ✓ Semantic network (knowledge graph spreading activation)\n";
    std::cout << "  ✓ Hybrid fusion (multi-source evidence combination)\n";
    std::cout << "  ✓ Hallucination detection (evidence validation)\n";
    std::cout << "  ✓ Explanation generation (transparent reasoning traces)\n";
    std::cout << "\n";
    
    std::cout << "Performance Characteristics:\n";
    std::cout << "  • Latency: <50ms p95 (target for full pipeline)\n";
    std::cout << "  • Throughput: 500+ QPS (target)\n";
    std::cout << "  • Accuracy: 92-95% (target with all enhancements)\n";
    std::cout << "  • Memory: ~2.5GB max (128 episodes + semantic graph)\n";
    std::cout << "\n";
    
    return 0;
}
