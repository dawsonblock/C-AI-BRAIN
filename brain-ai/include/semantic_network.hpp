#ifndef BRAIN_AI_SEMANTIC_NETWORK_HPP
#define BRAIN_AI_SEMANTIC_NETWORK_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <queue>
#include <optional>
#include <mutex>

namespace brain_ai {

// Node in semantic graph
struct SemanticNode {
    std::string concept;
    std::vector<float> embedding;  // Optional
    std::unordered_map<std::string, float> edges;  // target → weight
    float activation_level = 0.0f;
    
    // Default constructor needed for unordered_map::operator[]
    SemanticNode() = default;
    
    SemanticNode(const std::string& c, const std::vector<float>& emb = {})
        : concept(c), embedding(emb) {}
};

// Directed weighted graph with spreading activation
class SemanticNetwork {
public:
    SemanticNetwork() = default;
    
    // Add node (concept)
    void add_node(const std::string& concept,
                  const std::vector<float>& embedding = {});
    
    // Add edge (relation)
    void add_edge(const std::string& source,
                  const std::string& target,
                  float weight = 1.0f);
    
    // Spreading activation: BFS with exponential decay
    std::vector<std::pair<std::string, float>> spread_activation(
        const std::vector<std::string>& source_concepts,
        size_t max_hops = 3,
        float decay_factor = 0.7f,
        float activation_threshold = 0.1f
    );
    
    // Find related concepts by embedding similarity
    std::vector<std::string> find_similar_concepts(
        const std::vector<float>& query_embedding,
        size_t top_k = 10,
        float threshold = 0.6f
    ) const;
    
    // Decay all activations (for long-running systems)
    void decay_activations(float decay_rate = 0.9f);
    
    // Clear all activations
    void reset_activations();
    
    // Graph stats
    size_t num_nodes() const;
    size_t num_edges() const;
    
    // Get node (const)
    std::optional<const SemanticNode*> get_node(const std::string& concept) const;
    
private:
    std::unordered_map<std::string, SemanticNode> nodes_;
    mutable std::mutex mutex_;  // Thread safety
};

} // namespace brain_ai

#endif // BRAIN_AI_SEMANTIC_NETWORK_HPP
