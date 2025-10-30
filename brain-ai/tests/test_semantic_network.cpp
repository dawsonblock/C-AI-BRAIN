#include "semantic_network.hpp"
#include <cassert>
#include <iostream>

using namespace brain_ai;

void test_semantic_network() {
    // Test basic node and edge addition
    {
        SemanticNetwork network;
        
        network.add_node("A");
        network.add_node("B");
        network.add_edge("A", "B", 0.8f);
        
        assert(network.num_nodes() == 2 && "Should have 2 nodes");
        assert(network.num_edges() == 1 && "Should have 1 edge");
        
        auto node = network.get_node("A");
        assert(node.has_value() && "Node A should exist");
    }
    
    // Test spreading activation
    {
        SemanticNetwork network;
        
        network.add_node("A");
        network.add_node("B");
        network.add_node("C");
        network.add_edge("A", "B", 1.0f);
        network.add_edge("B", "C", 1.0f);
        
        auto activated = network.spread_activation({"A"}, 2, 0.7f, 0.1f);
        
        assert(!activated.empty() && "Should have activated nodes");
        
        // A should have highest activation (source)
        bool found_a = false;
        for (const auto& [concept, activation] : activated) {
            if (concept == "A") {
                found_a = true;
                assert(activation >= 0.9f && "Source should have high activation");
            }
        }
        assert(found_a && "Source node should be in results");
    }
    
    // Test find_similar_concepts
    {
        SemanticNetwork network;
        
        std::vector<float> emb1 = {1.0f, 0.0f, 0.0f};
        std::vector<float> emb2 = {0.9f, 0.1f, 0.0f};
        std::vector<float> emb3 = {0.0f, 1.0f, 0.0f};
        
        network.add_node("A", emb1);
        network.add_node("B", emb2);
        network.add_node("C", emb3);
        
        auto similar = network.find_similar_concepts(emb1, 2, 0.7f);
        
        assert(!similar.empty() && "Should find similar concepts");
        assert(similar[0] == "A" && "Most similar should be exact match");
    }
    
    // Test reset activations
    {
        SemanticNetwork network;
        
        network.add_node("A");
        network.add_node("B");
        network.add_edge("A", "B");
        
        network.spread_activation({"A"});
        network.reset_activations();
        
        auto node = network.get_node("A");
        assert(node.has_value() && "Node should exist");
        assert((*node)->activation_level == 0.0f && "Activation should be reset");
    }
    
    std::cout << "All semantic network tests passed!\n";
}
