#include "cognitive_handler.hpp"
#include "utils.hpp"
#include <algorithm>

namespace brain_ai {

CognitiveHandler::CognitiveHandler(
    size_t episodic_capacity,
    const FusionWeights& fusion_weights
) : episodic_buffer_(episodic_capacity),
    fusion_(fusion_weights) {
    // Initialize HNSWlib vector index with default OpenAI ada-002 dimension (1536)
    // Can be configured for other embedding models
    vector_index_ = std::make_unique<vector_search::HNSWIndex>(
        1536,  // Dimension (OpenAI ada-002)
        100000,  // Max elements
        16,      // M parameter
        200      // ef_construction
    );
    vector_index_->set_ef_search(50);  // Default search precision
}

QueryResponse CognitiveHandler::process_query(
    const std::string& query,
    const std::vector<float>& query_embedding,
    const QueryConfig& config
) {
    QueryResponse response(query);
    std::vector<ReasoningStep> reasoning_trace;
    
    // Step 1: Vector search (baseline - simulated here)
    auto vector_results = vector_search(query_embedding, config.top_k_results);
    
    if (!vector_results.empty()) {
        float avg_sim = 0.0f;
        std::vector<std::string> top_contents;
        for (size_t i = 0; i < std::min(size_t(3), vector_results.size()); ++i) {
            avg_sim += vector_results[i].score;
            top_contents.push_back(vector_results[i].content.substr(0, 50) + "...");
        }
        avg_sim /= std::min(size_t(3), vector_results.size());
        
        reasoning_trace.push_back(
            ExplanationEngine::create_vector_search_step(
                vector_results.size(), avg_sim, top_contents
            )
        );
    }
    
    // Step 2: Episodic buffer retrieval (if enabled)
    std::vector<ScoredResult> episodic_results;
    if (config.use_episodic && episodic_buffer_.size() > 0) {
        auto episodes = episodic_buffer_.retrieve_similar(query_embedding, 5, 0.6f);
        episodic_results = episodes_to_results(episodes);
        
        if (!episodic_results.empty()) {
            float avg_rel = 0.0f;
            std::vector<std::string> relevant_queries;
            for (size_t i = 0; i < std::min(size_t(2), episodic_results.size()); ++i) {
                avg_rel += episodic_results[i].score;
                relevant_queries.push_back(episodic_results[i].content.substr(0, 40) + "...");
            }
            avg_rel /= std::min(size_t(2), episodic_results.size());
            
            reasoning_trace.push_back(
                ExplanationEngine::create_episodic_step(
                    episodic_results.size(), avg_rel, relevant_queries
                )
            );
        }
    }
    
    // Step 3: Semantic network activation (if enabled)
    std::vector<ScoredResult> semantic_results;
    if (config.use_semantic && semantic_network_.num_nodes() > 0) {
        // Extract concepts from query
        auto query_concepts = extract_concepts(query);
        
        // Spread activation
        auto activated = semantic_network_.spread_activation(query_concepts, 3, 0.7f, 0.1f);
        
        // Convert to scored results
        for (const auto& [concept, activation] : activated) {
            semantic_results.push_back(ScoredResult(concept, activation, "semantic"));
        }
        
        if (!semantic_results.empty()) {
            std::vector<std::string> activated_concepts;
            for (size_t i = 0; i < std::min(size_t(5), semantic_results.size()); ++i) {
                activated_concepts.push_back(semantic_results[i].content);
            }
            
            float max_activation = semantic_results.empty() ? 0.0f : semantic_results[0].score;
            
            reasoning_trace.push_back(
                ExplanationEngine::create_semantic_step(
                    semantic_results.size(), max_activation, activated_concepts
                )
            );
        }
    }
    
    // Step 4: Hybrid fusion
    auto fused_results = fusion_.fuse(
        vector_results,
        episodic_results,
        semantic_results,
        config.top_k_results
    );
    
    response.results = fused_results;
    
    if (!fused_results.empty()) {
        auto weights = fusion_.get_weights();
        reasoning_trace.push_back(
            ExplanationEngine::create_fusion_step(
                weights.vector_weight,
                weights.episodic_weight,
                weights.semantic_weight,
                fused_results[0].score
            )
        );
        
        // Generate response from top result
        response.response = fused_results[0].content;
        response.overall_confidence = fused_results[0].score;
    } else {
        response.response = "No results found.";
        response.overall_confidence = 0.0f;
    }
    
    // Step 5: Hallucination detection (if enabled)
    if (config.check_hallucination && !response.response.empty()) {
        // Collect evidence from all sources
        std::vector<Evidence> evidence;
        
        for (const auto& result : vector_results) {
            evidence.push_back(Evidence("vector_search", result.score, result.content));
        }
        for (const auto& result : episodic_results) {
            evidence.push_back(Evidence("episodic_buffer", result.score, result.content));
        }
        for (const auto& result : semantic_results) {
            evidence.push_back(Evidence("semantic_network", result.score, result.content));
        }
        
        response.hallucination_check = hallucination_detector_.validate(
            query, response.response, evidence, config.hallucination_threshold
        );
        
        reasoning_trace.push_back(
            ExplanationEngine::create_hallucination_check_step(
                !response.hallucination_check.is_hallucination,
                response.hallucination_check.confidence_score,
                response.hallucination_check.flags
            )
        );
    }
    
    // Step 6: Generate explanation (if enabled)
    if (config.generate_explanation) {
        response.explanation = explanation_engine_.generate_explanation(
            query, response.response, reasoning_trace
        );
    }
    
    return response;
}

void CognitiveHandler::add_episode(
    const std::string& query,
    const std::string& response,
    const std::vector<float>& query_embedding,
    const std::unordered_map<std::string, std::string>& metadata
) {
    episodic_buffer_.add_episode(query, response, query_embedding, metadata);
}

void CognitiveHandler::populate_semantic_network(
    const std::vector<std::pair<std::string, std::vector<float>>>& concepts,
    const std::vector<std::tuple<std::string, std::string, float>>& relations
) {
    // Add concepts
    for (const auto& [concept, embedding] : concepts) {
        semantic_network_.add_node(concept, embedding);
    }
    
    // Add relations
    for (const auto& [source, target, weight] : relations) {
        semantic_network_.add_edge(source, target, weight);
    }
}

// Real vector search using HNSWlib
std::vector<ScoredResult> CognitiveHandler::vector_search(
    const std::vector<float>& query_embedding,
    size_t top_k
) {
    // Query the HNSW index for nearest neighbors
    auto hnsw_results = vector_index_->search(query_embedding, top_k);
    
    // Convert HNSWlib results to ScoredResult format
    std::vector<ScoredResult> results;
    results.reserve(hnsw_results.size());
    
    for (const auto& result : hnsw_results) {
        results.push_back(ScoredResult(
            result.content,
            result.similarity,
            "vector"
        ));
    }
    
    return results;
}

std::vector<ScoredResult> CognitiveHandler::episodes_to_results(
    const std::vector<Episode>& episodes
) {
    std::vector<ScoredResult> results;
    results.reserve(episodes.size());
    
    for (const auto& episode : episodes) {
        // Use response as content
        std::string content = "Previous context: Q: " + episode.query + 
                             " A: " + episode.response;
        
        // Compute score based on timestamp decay (episodes already scored in retrieval)
        float score = 0.8f;  // Placeholder - real implementation would use actual similarity
        
        results.push_back(ScoredResult(content, score, "episodic"));
    }
    
    return results;
}

std::vector<std::string> CognitiveHandler::extract_concepts(const std::string& query) {
    // Simple tokenization and filtering
    // Real implementation would use NLP/NER
    
    std::vector<std::string> concepts;
    auto tokens = tokenize(to_lowercase(query));
    
    // Filter out stopwords (simplified)
    std::unordered_set<std::string> stopwords = {
        "the", "is", "at", "which", "on", "a", "an", "and", "or", "but",
        "in", "with", "to", "for", "of", "as", "by", "from", "how", "what",
        "where", "when", "why", "who"
    };
    
    for (const auto& token : tokens) {
        if (token.length() > 3 && stopwords.find(token) == stopwords.end()) {
            concepts.push_back(token);
        }
    }
    
    return concepts;
}

bool CognitiveHandler::index_document(
    const std::string& doc_id,
    const std::vector<float>& embedding,
    const std::string& content,
    const nlohmann::json& metadata
) {
    return vector_index_->add_document(doc_id, embedding, content, metadata);
}

void CognitiveHandler::batch_index_documents(
    const std::vector<std::tuple<std::string, std::vector<float>, std::string>>& documents
) {
    for (const auto& [doc_id, embedding, content] : documents) {
        vector_index_->add_document(doc_id, embedding, content);
    }
}

} // namespace brain_ai
