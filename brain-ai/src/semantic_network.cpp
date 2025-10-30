#include "semantic_network.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace brain_ai {

void SemanticNetwork::add_node(const std::string& concept,
                                const std::vector<float>& embedding) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nodes_.find(concept) == nodes_.end()) {
        nodes_.emplace(concept, SemanticNode(concept, embedding));
    }
}

void SemanticNetwork::add_edge(const std::string& source,
                                const std::string& target,
                                float weight) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Ensure nodes exist
    if (nodes_.find(source) == nodes_.end()) {
        nodes_.emplace(source, SemanticNode(source));
    }
    if (nodes_.find(target) == nodes_.end()) {
        nodes_.emplace(target, SemanticNode(target));
    }
    
    // Add edge
    nodes_[source].edges[target] = weight;
}

std::vector<std::pair<std::string, float>> SemanticNetwork::spread_activation(
    const std::vector<std::string>& source_concepts,
    size_t max_hops,
    float decay_factor,
    float activation_threshold
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reset activations
    for (auto& [concept, node] : nodes_) {
        node.activation_level = 0.0f;
    }
    
    // Activation map
    std::unordered_map<std::string, float> activations;
    
    // BFS queue: (concept, hop_count, activation_level)
    std::queue<std::tuple<std::string, size_t, float>> frontier;
    
    // Visited set to avoid cycles
    std::unordered_set<std::string> visited;
    
    // Initialize with source concepts
    for (const auto& concept : source_concepts) {
        if (nodes_.find(concept) != nodes_.end()) {
            frontier.push({concept, 0, 1.0f});
            activations[concept] = 1.0f;
            visited.insert(concept);
        }
    }
    
    // BFS with spreading activation
    while (!frontier.empty()) {
        auto [current, hops, activation] = frontier.front();
        frontier.pop();
        
        // Stop if max hops reached
        if (hops >= max_hops) {
            continue;
        }
        
        // Get current node
        auto it = nodes_.find(current);
        if (it == nodes_.end()) {
            continue;
        }
        
        const SemanticNode& node = it->second;
        
        // Spread activation to neighbors
        for (const auto& [neighbor, edge_weight] : node.edges) {
            // Compute decayed activation
            float new_activation = activation * decay_factor * edge_weight;
            
            // Skip if below threshold
            if (new_activation < activation_threshold) {
                continue;
            }
            
            // Update activation (accumulate if already activated)
            if (activations.find(neighbor) != activations.end()) {
                activations[neighbor] = std::max(activations[neighbor], new_activation);
            } else {
                activations[neighbor] = new_activation;
            }
            
            // Add to frontier if not visited at this level
            if (visited.find(neighbor) == visited.end()) {
                frontier.push({neighbor, hops + 1, new_activation});
                visited.insert(neighbor);
            }
        }
    }
    
    // Update node activation levels
    for (auto& [concept, activation] : activations) {
        if (nodes_.find(concept) != nodes_.end()) {
            nodes_[concept].activation_level = activation;
        }
    }
    
    // Convert to sorted vector
    std::vector<std::pair<std::string, float>> results;
    results.reserve(activations.size());
    
    for (const auto& [concept, activation] : activations) {
        results.push_back({concept, activation});
    }
    
    // Sort by activation level (descending)
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
    
    return results;
}

std::vector<std::string> SemanticNetwork::find_similar_concepts(
    const std::vector<float>& query_embedding,
    size_t top_k,
    float threshold
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    struct ScoredConcept {
        std::string concept;
        float similarity;
    };
    
    std::vector<ScoredConcept> scored_concepts;
    
    for (const auto& [concept, node] : nodes_) {
        // Skip nodes without embeddings
        if (node.embedding.empty()) {
            continue;
        }
        
        // Compute similarity
        float similarity = cosine_similarity(query_embedding, node.embedding);
        
        // Filter by threshold
        if (similarity >= threshold) {
            scored_concepts.push_back({concept, similarity});
        }
    }
    
    // Sort by similarity (descending)
    std::sort(scored_concepts.begin(), scored_concepts.end(),
        [](const ScoredConcept& a, const ScoredConcept& b) {
            return a.similarity > b.similarity;
        });
    
    // Take top-k
    std::vector<std::string> results;
    results.reserve(std::min(top_k, scored_concepts.size()));
    
    for (size_t i = 0; i < std::min(top_k, scored_concepts.size()); ++i) {
        results.push_back(scored_concepts[i].concept);
    }
    
    return results;
}

void SemanticNetwork::decay_activations(float decay_rate) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [concept, node] : nodes_) {
        node.activation_level *= decay_rate;
    }
}

void SemanticNetwork::reset_activations() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [concept, node] : nodes_) {
        node.activation_level = 0.0f;
    }
}

size_t SemanticNetwork::num_nodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

size_t SemanticNetwork::num_edges() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [concept, node] : nodes_) {
        count += node.edges.size();
    }
    
    return count;
}

std::optional<const SemanticNode*> SemanticNetwork::get_node(const std::string& concept) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(concept);
    if (it != nodes_.end()) {
        return &(it->second);
    }
    
    return std::nullopt;
}

} // namespace brain_ai
