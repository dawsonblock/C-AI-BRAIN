#pragma once

#include "vector_search/hnsw_index.hpp"
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include "nlohmann/json.hpp"

namespace brain_ai::indexing {

/**
 * @brief Statistics for index manager
 */
struct IndexStats {
    size_t total_documents = 0;
    size_t total_vectors = 0;
    size_t index_size_bytes = 0;
    std::chrono::system_clock::time_point last_update;
    std::chrono::system_clock::time_point created_at;
    
    IndexStats() : created_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief Batch operation result
 */
struct BatchResult {
    size_t total = 0;
    size_t successful = 0;
    size_t failed = 0;
    std::vector<std::string> error_messages;
    std::chrono::milliseconds total_time{0};
    
    float success_rate() const {
        return total > 0 ? static_cast<float>(successful) / total : 0.0f;
    }
};

/**
 * @brief Configuration for index manager
 */
struct IndexConfig {
    size_t embedding_dim = 1536;
    size_t max_elements = 100000;
    size_t M = 16;
    size_t ef_construction = 200;
    size_t ef_search = 50;
    std::string space_type = "ip";
    
    // Persistence
    std::string index_path = "";
    bool auto_save = true;
    std::chrono::seconds save_interval{300};  // 5 minutes
    
    // Batch processing
    size_t batch_size = 100;
    int num_threads = 4;
    
    IndexConfig() = default;
};

/**
 * @brief Enhanced index manager with batch operations and persistence
 * 
 * Provides high-level interface for document indexing with:
 * - Batch operations with parallel processing
 * - Automatic persistence
 * - Document metadata tracking
 * - Index statistics
 * - Transaction-like operations
 * 
 * Thread-safe: All methods use mutex protection.
 * 
 * Example usage:
 * @code
 *   IndexConfig config;
 *   config.embedding_dim = 1536;
 *   config.index_path = "/path/to/index";
 *   config.auto_save = true;
 *   
 *   IndexManager manager(config);
 *   
 *   // Batch add documents
 *   std::vector<std::string> doc_ids;
 *   std::vector<std::vector<float>> embeddings;
 *   std::vector<std::string> contents;
 *   
 *   auto result = manager.add_batch(doc_ids, embeddings, contents);
 *   std::cout << "Success rate: " << result.success_rate() * 100 << "%" << std::endl;
 *   
 *   // Search
 *   auto results = manager.search(query_embedding, 10);
 *   
 *   // Save index
 *   manager.save();
 * @endcode
 */
class IndexManager {
public:
    /**
     * @brief Construct index manager
     * @param config Configuration
     */
    explicit IndexManager(const IndexConfig& config);
    
    /**
     * @brief Destructor - saves index if auto_save enabled
     */
    ~IndexManager();
    
    // Non-copyable and non-movable
    IndexManager(const IndexManager&) = delete;
    IndexManager& operator=(const IndexManager&) = delete;
    IndexManager(IndexManager&&) = delete;
    IndexManager& operator=(IndexManager&&) = delete;
    
    /**
     * @brief Add single document
     * @param doc_id Document identifier
     * @param embedding Embedding vector
     * @param content Document content
     * @param metadata Document metadata
     * @return true if successful
     */
    bool add_document(const std::string& doc_id,
                     const std::vector<float>& embedding,
                     const std::string& content,
                     const nlohmann::json& metadata = {});
    
    /**
     * @brief Add multiple documents in batch
     * @param doc_ids Document identifiers
     * @param embeddings Embedding vectors
     * @param contents Document contents
     * @param metadatas Document metadata (optional)
     * @return Batch operation result
     */
    BatchResult add_batch(const std::vector<std::string>& doc_ids,
                         const std::vector<std::vector<float>>& embeddings,
                         const std::vector<std::string>& contents,
                         const std::vector<nlohmann::json>& metadatas = {});
    
    /**
     * @brief Search for similar documents
     * @param query_embedding Query vector
     * @param top_k Number of results
     * @param similarity_threshold Minimum similarity score
     * @return Search results
     */
    std::vector<vector_search::SearchResult> search(
        const std::vector<float>& query_embedding,
        size_t top_k = 10,
        float similarity_threshold = 0.0f);
    
    /**
     * @brief Batch search multiple queries
     * @param query_embeddings Query vectors
     * @param top_k Number of results per query
     * @return Search results for each query
     */
    std::vector<std::vector<vector_search::SearchResult>> search_batch(
        const std::vector<std::vector<float>>& query_embeddings,
        size_t top_k = 10);
    
    /**
     * @brief Delete document by ID
     * @param doc_id Document identifier
     * @return true if deleted
     */
    bool delete_document(const std::string& doc_id);
    
    /**
     * @brief Update document
     * @param doc_id Document identifier
     * @param embedding New embedding
     * @param content New content
     * @param metadata New metadata
     * @return true if updated
     */
    bool update_document(const std::string& doc_id,
                        const std::vector<float>& embedding,
                        const std::string& content,
                        const nlohmann::json& metadata = {});
    
    /**
     * @brief Get document by ID
     * @param doc_id Document identifier
     * @return Document metadata or empty if not found
     */
    nlohmann::json get_document(const std::string& doc_id) const;
    
    /**
     * @brief Check if document exists
     * @param doc_id Document identifier
     * @return true if exists
     */
    bool has_document(const std::string& doc_id) const;
    
    /**
     * @brief Get total number of documents
     * @return Document count
     */
    size_t document_count() const;
    
    /**
     * @brief Save index to disk
     * @return true if successful
     */
    bool save();
    
    /**
     * @brief Load index from disk
     * @return true if successful
     */
    bool load();
    
    /**
     * @brief Clear all documents
     */
    void clear();
    
    /**
     * @brief Get index statistics
     * @return Current stats
     */
    IndexStats get_stats() const;
    
    /**
     * @brief Update search parameters
     * @param ef_search EF parameter for search
     */
    void set_ef_search(size_t ef_search);
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const IndexConfig& get_config() const { return config_; }

private:
    IndexConfig config_;
    std::unique_ptr<vector_search::HNSWIndex> index_;
    
    mutable std::mutex mutex_;
    
    // Document metadata storage
    std::unordered_map<std::string, nlohmann::json> documents_;
    
    // Statistics
    IndexStats stats_;
    
    // Auto-save tracking
    std::chrono::steady_clock::time_point last_save_;
    
    /**
     * @brief Check if auto-save is needed
     * @return true if should save
     */
    bool should_auto_save() const;
    
    /**
     * @brief Update statistics
     */
    void update_stats();
    
    /**
     * @brief Generate document metadata
     * @param doc_id Document ID
     * @param content Content
     * @param user_metadata User-provided metadata
     * @return Complete metadata object
     */
    nlohmann::json create_metadata(const std::string& doc_id,
                                   const std::string& content,
                                   const nlohmann::json& user_metadata) const;
};

} // namespace brain_ai::indexing
