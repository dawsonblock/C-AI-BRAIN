#include "episodic_buffer.hpp"
#include "utils.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>

namespace brain_ai {

// Episode implementation
Episode::Episode(const std::string& q, 
                 const std::string& r,
                 const std::vector<float>& emb,
                 uint64_t ts,
                 const std::unordered_map<std::string, std::string>& meta)
    : query(q), response(r), query_embedding(emb), 
      timestamp_ms(ts > 0 ? ts : current_timestamp_ms()),
      metadata(meta) {}

uint64_t Episode::current_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// EpisodicBuffer implementation
EpisodicBuffer::EpisodicBuffer(size_t capacity) 
    : max_capacity_(capacity) {}

void EpisodicBuffer::add_episode(
    const std::string& query,
    const std::string& response,
    const std::vector<float>& query_embedding,
    const std::unordered_map<std::string, std::string>& metadata
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create episode
    Episode episode(query, response, query_embedding, 
                    Episode::current_timestamp_ms(), metadata);
    
    // Add to buffer
    buffer_.push_back(std::move(episode));
    
    // Evict oldest if full
    if (buffer_.size() > max_capacity_) {
        buffer_.pop_front();
    }
}

std::vector<Episode> EpisodicBuffer::retrieve_similar(
    const std::vector<float>& query_embedding,
    size_t top_k,
    float similarity_threshold
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Compute similarity + temporal decay for each episode
    struct ScoredEpisode {
        const Episode* episode;
        float score;
    };
    
    std::vector<ScoredEpisode> scored_episodes;
    scored_episodes.reserve(buffer_.size());
    
    uint64_t current_time = Episode::current_timestamp_ms();
    
    for (const auto& episode : buffer_) {
        // Cosine similarity
        float similarity = cosine_similarity(query_embedding, 
                                             episode.query_embedding);
        
        // Temporal decay
        float decay = compute_temporal_decay(episode.timestamp_ms, 
                                             current_time);
        
        // Combined score
        float score = similarity * decay;
        
        // Filter by threshold
        if (score >= similarity_threshold) {
            scored_episodes.push_back({&episode, score});
        }
    }
    
    // Sort by score (descending)
    std::sort(scored_episodes.begin(), scored_episodes.end(),
        [](const ScoredEpisode& a, const ScoredEpisode& b) {
            return a.score > b.score;
        });
    
    // Take top-k
    std::vector<Episode> results;
    results.reserve(std::min(top_k, scored_episodes.size()));
    
    for (size_t i = 0; i < std::min(top_k, scored_episodes.size()); ++i) {
        results.push_back(*scored_episodes[i].episode);
    }
    
    return results;
}

std::vector<Episode> EpisodicBuffer::get_recent(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Episode> results;
    results.reserve(std::min(count, buffer_.size()));
    
    // Take from end (most recent)
    auto start = buffer_.size() > count ? buffer_.end() - count : buffer_.begin();
    results.insert(results.end(), start, buffer_.end());
    
    return results;
}

void EpisodicBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
}

size_t EpisodicBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

bool EpisodicBuffer::is_full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size() >= max_capacity_;
}

float EpisodicBuffer::compute_temporal_decay(
    uint64_t episode_timestamp_ms,
    uint64_t current_timestamp_ms,
    float lambda
) const {
    float time_delta = static_cast<float>(current_timestamp_ms - episode_timestamp_ms);
    return std::exp(-lambda * time_delta);
}

// Simple persistence (CSV format for simplicity)
void EpisodicBuffer::save_to_file(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    
    // Write header
    ofs << "query,response,timestamp_ms,embedding_dim\n";
    
    // Write episodes
    for (const auto& episode : buffer_) {
        ofs << episode.query << "," 
            << episode.response << ","
            << episode.timestamp_ms << ","
            << episode.query_embedding.size() << "\n";
    }
}

void EpisodicBuffer::load_from_file(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + filepath);
    }
    
    buffer_.clear();
    
    // Skip header
    std::string line;
    std::getline(ifs, line);
    
    // Read episodes (simplified - real implementation would handle embeddings)
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string query, response, timestamp_str, dim_str;
        
        std::getline(iss, query, ',');
        std::getline(iss, response, ',');
        std::getline(iss, timestamp_str, ',');
        std::getline(iss, dim_str, ',');
        
        uint64_t timestamp = std::stoull(timestamp_str);
        size_t dim = std::stoull(dim_str);
        
        // Create dummy embedding (real implementation would save/load embeddings)
        std::vector<float> embedding(dim, 0.0f);
        
        buffer_.emplace_back(query, response, embedding, timestamp);
    }
}

} // namespace brain_ai
