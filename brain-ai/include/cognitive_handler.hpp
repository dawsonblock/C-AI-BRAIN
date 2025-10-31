#ifndef BRAIN_AI_COGNITIVE_HANDLER_HPP
#define BRAIN_AI_COGNITIVE_HANDLER_HPP

#include "episodic_buffer.hpp"
#include "semantic_network.hpp"
#include "hallucination_detector.hpp"
#include "hybrid_fusion.hpp"
#include "explanation_engine.hpp"
#include "vector_search/hnsw_index.hpp"
#include <memory>
#include <string>
#include <vector>

namespace brain_ai {

// Query configuration
struct QueryConfig {
    bool use_episodic = true;
    bool use_semantic = true;
    bool check_hallucination = true;
    bool generate_explanation = true;
    size_t top_k_results = 10;
    float hallucination_threshold = 0.5f;
};

// Complete query response
struct QueryResponse {
    std::string query;
    std::string response;
    std::vector<ScoredResult> results;
    HallucinationResult hallucination_check;
    Explanation explanation;
    float overall_confidence;
    
    QueryResponse(const std::string& q = "")
        : query(q), overall_confidence(0.0f) {}
};

// Main cognitive architecture orchestrator
class CognitiveHandler {
public:
    CognitiveHandler(
        size_t episodic_capacity = 128,
        const FusionWeights& fusion_weights = FusionWeights(),
        size_t embedding_dim = 1536
    );
    
    // Process a query through the full cognitive pipeline
    QueryResponse process_query(
        const std::string& query,
        const std::vector<float>& query_embedding,
        const QueryConfig& config = QueryConfig()
    );
    
    // Add episode after response is generated
    void add_episode(
        const std::string& query,
        const std::string& response,
        const std::vector<float>& query_embedding,
        const std::unordered_map<std::string, std::string>& metadata = {}
    );
    
    // Index a document for vector search
    bool index_document(
        const std::string& doc_id,
        const std::vector<float>& embedding,
        const std::string& content,
        const nlohmann::json& metadata = {}
    );
    
    // Batch index documents
    void batch_index_documents(
        const std::vector<std::tuple<std::string, std::vector<float>, std::string>>& documents
    );
    
    // Populate semantic network with domain knowledge
    void populate_semantic_network(
        const std::vector<std::pair<std::string, std::vector<float>>>& concepts,
        const std::vector<std::tuple<std::string, std::string, float>>& relations
    );
    
    // Get components for direct access (optional)
    EpisodicBuffer& episodic_buffer() { return episodic_buffer_; }
    SemanticNetwork& semantic_network() { return semantic_network_; }
    HybridFusion& fusion() { return fusion_; }
    vector_search::HNSWIndex& vector_index() { return *vector_index_; }
    
    // Statistics
    size_t episodic_buffer_size() const { return episodic_buffer_.size(); }
    size_t semantic_network_size() const { return semantic_network_.num_nodes(); }
    size_t vector_index_size() const { return vector_index_->size(); }
    
private:
    EpisodicBuffer episodic_buffer_;
    SemanticNetwork semantic_network_;
    HallucinationDetector hallucination_detector_;
    HybridFusion fusion_;
    ExplanationEngine explanation_engine_;
    std::unique_ptr<vector_search::HNSWIndex> vector_index_;
    size_t embedding_dim_;
    
    // Real vector search using HNSWlib
    std::vector<ScoredResult> vector_search(
        const std::vector<float>& query_embedding,
        size_t top_k
    );
    
    // Convert episodes to scored results
    std::vector<ScoredResult> episodes_to_results(
        const std::vector<Episode>& episodes
    );
    
    // Extract concepts from query
    std::vector<std::string> extract_concepts(const std::string& query);
};

} // namespace brain_ai

#endif // BRAIN_AI_COGNITIVE_HANDLER_HPP
