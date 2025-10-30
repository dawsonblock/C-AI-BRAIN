#ifndef BRAIN_AI_EPISODIC_BUFFER_HPP
#define BRAIN_AI_EPISODIC_BUFFER_HPP

#include <deque>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>

namespace brain_ai {

// Single episode in memory
struct Episode {
    std::string query;
    std::string response;
    std::vector<float> query_embedding;
    uint64_t timestamp_ms;
    std::unordered_map<std::string, std::string> metadata;
    
    // Constructor
    Episode(const std::string& q, 
            const std::string& r,
            const std::vector<float>& emb,
            uint64_t ts = 0,
            const std::unordered_map<std::string, std::string>& meta = {});
    
    // Get current timestamp in milliseconds
    static uint64_t current_timestamp_ms();
};

// Fixed-capacity ring buffer for conversation context
class EpisodicBuffer {
public:
    // Constructor
    explicit EpisodicBuffer(size_t capacity = 128);
    
    // Add new episode (auto-evicts oldest if full)
    void add_episode(const std::string& query,
                     const std::string& response,
                     const std::vector<float>& query_embedding,
                     const std::unordered_map<std::string, std::string>& metadata = {});
    
    // Retrieve k most similar past episodes
    std::vector<Episode> retrieve_similar(
        const std::vector<float>& query_embedding,
        size_t top_k = 5,
        float similarity_threshold = 0.7f
    ) const;
    
    // Get recent episodes by time
    std::vector<Episode> get_recent(size_t count) const;
    
    // Clear all episodes
    void clear();
    
    // Persistence
    void save_to_file(const std::string& filepath) const;
    void load_from_file(const std::string& filepath);
    
    // Stats
    size_t size() const;
    bool is_full() const;
    size_t capacity() const { return max_capacity_; }
    
private:
    std::deque<Episode> buffer_;
    size_t max_capacity_;
    mutable std::mutex mutex_;  // Thread safety for add/retrieve
    
    // Helper: Compute temporal decay
    float compute_temporal_decay(uint64_t episode_timestamp_ms,
                                  uint64_t current_timestamp_ms,
                                  float lambda = 1e-6f) const;
};

} // namespace brain_ai

#endif // BRAIN_AI_EPISODIC_BUFFER_HPP
