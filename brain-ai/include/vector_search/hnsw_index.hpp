#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <hnswlib/hnswlib.h>

namespace brain_ai {
namespace vector_search {

/**
 * SearchResult represents a single search result with document information
 */
struct SearchResult {
    std::string doc_id;          // Document identifier
    std::string content;         // Document content
    float similarity;            // Cosine similarity score (0.0 to 1.0)
    nlohmann::json metadata;     // Optional metadata
    
    SearchResult() : similarity(0.0f) {}
    
    SearchResult(const std::string& id, const std::string& text, 
                float sim, const nlohmann::json& meta = {})
        : doc_id(id), content(text), similarity(sim), metadata(meta) {}
};

/**
 * DocumentMetadata stores information about indexed documents
 */
struct DocumentMetadata {
    std::string doc_id;
    std::string content;
    nlohmann::json metadata;
    size_t internal_id;  // HNSWlib internal ID
    
    DocumentMetadata() : internal_id(0) {}
    
    DocumentMetadata(const std::string& id, const std::string& text,
                    const nlohmann::json& meta, size_t iid)
        : doc_id(id), content(text), metadata(meta), internal_id(iid) {}
};

/**
 * IndexStatistics provides metrics about the index
 */
struct IndexStatistics {
    size_t total_documents = 0;
    size_t dimension = 0;
    size_t max_elements = 0;
    size_t current_elements = 0;
    size_t M = 0;                  // HNSWlib M parameter
    size_t ef_construction = 0;    // HNSWlib ef_construction parameter
    size_t ef_search = 0;          // Current ef_search parameter
    double memory_usage_mb = 0.0;
    
    nlohmann::json to_json() const {
        return {
            {"total_documents", total_documents},
            {"dimension", dimension},
            {"max_elements", max_elements},
            {"current_elements", current_elements},
            {"M", M},
            {"ef_construction", ef_construction},
            {"ef_search", ef_search},
            {"memory_usage_mb", memory_usage_mb}
        };
    }
};

/**
 * HNSWIndex provides efficient approximate nearest neighbor search
 * using the HNSW (Hierarchical Navigable Small World) algorithm.
 * 
 * Features:
 * - Fast approximate nearest neighbor search (sub-linear time)
 * - Thread-safe operations
 * - Save/load index to/from disk
 * - Metadata storage for documents
 * - Configurable precision-recall tradeoff
 * 
 * Usage:
 *   HNSWIndex index(1536);  // OpenAI ada-002 dimension
 *   index.add_document("doc1", embedding, "Document content");
 *   auto results = index.search(query_embedding, 10);
 */
class HNSWIndex {
public:
    /**
     * Constructor
     * @param dim Embedding dimension (e.g., 1536 for OpenAI ada-002)
     * @param max_elements Maximum number of documents to index
     * @param M HNSW parameter - number of connections per layer (default: 16)
     * @param ef_construction HNSW parameter - size of dynamic candidate list (default: 200)
     * @param space_type Distance metric: "l2" (Euclidean) or "ip" (Inner Product/Cosine)
     */
    explicit HNSWIndex(size_t dim, 
                      size_t max_elements = 100000,
                      size_t M = 16,
                      size_t ef_construction = 200,
                      const std::string& space_type = "ip");
    
    /**
     * Destructor - cleans up HNSWlib index
     */
    ~HNSWIndex();
    
    // Disable copy
    HNSWIndex(const HNSWIndex&) = delete;
    HNSWIndex& operator=(const HNSWIndex&) = delete;
    
    // Enable move
    HNSWIndex(HNSWIndex&& other) noexcept;
    HNSWIndex& operator=(HNSWIndex&& other) noexcept;
    
    /**
     * Add a document to the index
     * @param doc_id Unique document identifier
     * @param embedding Document embedding vector
     * @param content Document text content
     * @param metadata Optional JSON metadata
     * @return true if added successfully, false if doc_id already exists
     */
    bool add_document(const std::string& doc_id,
                     const std::vector<float>& embedding,
                     const std::string& content,
                     const nlohmann::json& metadata = {});
    
    /**
     * Search for similar documents
     * @param query Query embedding vector
     * @param top_k Number of results to return
     * @return Vector of search results sorted by similarity (highest first)
     */
    std::vector<SearchResult> search(const std::vector<float>& query,
                                    size_t top_k = 10);
    
    /**
     * Remove a document from the index
     * @param doc_id Document identifier to remove
     * @return true if removed, false if not found
     */
    bool remove_document(const std::string& doc_id);
    
    /**
     * Check if a document exists in the index
     * @param doc_id Document identifier
     * @return true if exists
     */
    bool has_document(const std::string& doc_id) const;
    
    /**
     * Get document metadata by ID
     * @param doc_id Document identifier
     * @return Document metadata, or empty metadata if not found
     */
    DocumentMetadata get_document(const std::string& doc_id) const;
    
    /**
     * Save index to disk
     * @param filepath Path to save index file
     * @return true if saved successfully
     */
    bool save(const std::string& filepath);
    
    /**
     * Load index from disk
     * @param filepath Path to index file
     * @return true if loaded successfully
     */
    bool load(const std::string& filepath);
    
    /**
     * Clear all documents from the index
     */
    void clear();
    
    /**
     * Get current number of documents
     * @return Document count
     */
    size_t size() const;
    
    /**
     * Get index statistics
     * @return Statistics structure
     */
    IndexStatistics get_statistics() const;
    
    /**
     * Set ef parameter for search (precision-recall tradeoff)
     * Higher values = more accurate but slower search
     * @param ef Search parameter (default: 50, typical range: 10-500)
     */
    void set_ef_search(size_t ef);
    
    /**
     * Get current ef_search parameter
     * @return Current ef value
     */
    size_t get_ef_search() const;

private:
    size_t dim_;                    // Embedding dimension
    size_t max_elements_;           // Maximum capacity
    size_t M_;                      // HNSW M parameter
    size_t ef_construction_;        // HNSW ef_construction parameter
    size_t ef_search_;              // Current search precision parameter
    std::string space_type_;        // Distance metric type
    
    // HNSWlib index (using Inner Product space for cosine similarity)
    std::unique_ptr<hnswlib::HierarchicalNSW<float>> index_;
    std::unique_ptr<hnswlib::SpaceInterface<float>> space_;
    
    // Document metadata storage
    std::unordered_map<std::string, DocumentMetadata> documents_;
    std::unordered_map<size_t, std::string> internal_id_to_doc_id_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Internal ID counter
    size_t next_internal_id_;
    
    /**
     * Initialize HNSWlib index
     */
    void initialize_index();
    
    /**
     * Normalize vector for cosine similarity (when using Inner Product space)
     * @param vec Vector to normalize
     */
    void normalize_vector(std::vector<float>& vec) const;
    
    /**
     * Convert Inner Product distance to cosine similarity
     * @param ip_distance Inner product distance from HNSWlib
     * @return Cosine similarity in [0, 1]
     */
    float ip_to_similarity(float ip_distance) const;
};

/**
 * IndexBuilder provides a fluent interface for constructing HNSWIndex
 * 
 * Usage:
 *   auto index = IndexBuilder()
 *       .dimension(1536)
 *       .max_elements(100000)
 *       .M(16)
 *       .ef_construction(200)
 *       .build();
 */
class IndexBuilder {
public:
    IndexBuilder() : dim_(768), max_elements_(100000), 
                    M_(16), ef_construction_(200), space_type_("ip") {}
    
    IndexBuilder& dimension(size_t dim) { dim_ = dim; return *this; }
    IndexBuilder& max_elements(size_t max) { max_elements_ = max; return *this; }
    IndexBuilder& M(size_t m) { M_ = m; return *this; }
    IndexBuilder& ef_construction(size_t ef) { ef_construction_ = ef; return *this; }
    IndexBuilder& space_type(const std::string& type) { space_type_ = type; return *this; }
    
    std::unique_ptr<HNSWIndex> build() {
        return std::make_unique<HNSWIndex>(dim_, max_elements_, M_, 
                                          ef_construction_, space_type_);
    }

private:
    size_t dim_;
    size_t max_elements_;
    size_t M_;
    size_t ef_construction_;
    std::string space_type_;
};

} // namespace vector_search
} // namespace brain_ai
