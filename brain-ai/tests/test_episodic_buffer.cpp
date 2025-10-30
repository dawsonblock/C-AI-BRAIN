#include "episodic_buffer.hpp"
#include <cassert>
#include <iostream>

using namespace brain_ai;

void test_episodic_buffer() {
    // Test basic add and retrieve
    {
        EpisodicBuffer buffer(10);
        std::vector<float> emb1 = {1.0f, 0.0f, 0.0f};
        std::vector<float> emb2 = {0.9f, 0.1f, 0.0f};
        
        buffer.add_episode("query1", "response1", emb1);
        buffer.add_episode("query2", "response2", emb2);
        
        assert(buffer.size() == 2 && "Buffer should have 2 episodes");
        
        // Retrieve similar
        auto similar = buffer.retrieve_similar(emb1, 5, 0.5f);
        assert(!similar.empty() && "Should find similar episodes");
        assert(similar[0].query == "query1" && "First result should be exact match");
    }
    
    // Test capacity limit
    {
        EpisodicBuffer buffer(3);
        std::vector<float> emb = {1.0f, 0.0f, 0.0f};
        
        buffer.add_episode("q1", "r1", emb);
        buffer.add_episode("q2", "r2", emb);
        buffer.add_episode("q3", "r3", emb);
        buffer.add_episode("q4", "r4", emb);  // Should evict q1
        
        assert(buffer.size() == 3 && "Buffer should be at capacity");
        assert(buffer.is_full() && "Buffer should be full");
        
        // Oldest should be evicted
        auto all = buffer.get_recent(10);
        assert(all[0].query != "q1" && "Oldest episode should be evicted");
    }
    
    // Test get_recent
    {
        EpisodicBuffer buffer(10);
        std::vector<float> emb = {1.0f, 0.0f, 0.0f};
        
        buffer.add_episode("q1", "r1", emb);
        buffer.add_episode("q2", "r2", emb);
        buffer.add_episode("q3", "r3", emb);
        
        auto recent = buffer.get_recent(2);
        assert(recent.size() == 2 && "Should get 2 recent episodes");
        assert(recent[1].query == "q3" && "Most recent should be last");
    }
    
    // Test clear
    {
        EpisodicBuffer buffer(10);
        std::vector<float> emb = {1.0f, 0.0f, 0.0f};
        
        buffer.add_episode("q1", "r1", emb);
        buffer.clear();
        
        assert(buffer.size() == 0 && "Buffer should be empty after clear");
    }
    
    std::cout << "All episodic buffer tests passed!\n";
}
