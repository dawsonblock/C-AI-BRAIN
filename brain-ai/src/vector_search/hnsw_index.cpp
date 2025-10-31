#include "vector_search/hnsw_index.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace brain_ai {
namespace vector_search {

// ============================================================================
// HNSWIndex Implementation
// ============================================================================

HNSWIndex::HNSWIndex(size_t dim, size_t max_elements, size_t M, 
                     size_t ef_construction, const std::string& space_type)
    : dim_(dim)
    , max_elements_(max_elements)
    , M_(M)
    , ef_construction_(ef_construction)
    , ef_search_(50)  // Default search parameter
    , space_type_(space_type)
    , next_internal_id_(0) {
    
    if (dim == 0) {
        throw std::invalid_argument("Dimension must be greater than 0");
    }
    
    if (max_elements == 0) {
        throw std::invalid_argument("Max elements must be greater than 0");
    }
    
    initialize_index();
}

HNSWIndex::~HNSWIndex() {
    // Smart pointers will handle cleanup
}

HNSWIndex::HNSWIndex(HNSWIndex&& other) noexcept
    : dim_(other.dim_)
    , max_elements_(other.max_elements_)
    , M_(other.M_)
    , ef_construction_(other.ef_construction_)
    , ef_search_(other.ef_search_)
    , space_type_(std::move(other.space_type_))
    , index_(std::move(other.index_))
    , space_(std::move(other.space_))
    , documents_(std::move(other.documents_))
    , internal_id_to_doc_id_(std::move(other.internal_id_to_doc_id_))
    , next_internal_id_(other.next_internal_id_) {
}

HNSWIndex& HNSWIndex::operator=(HNSWIndex&& other) noexcept {
    if (this != &other) {
        dim_ = other.dim_;
        max_elements_ = other.max_elements_;
        M_ = other.M_;
        ef_construction_ = other.ef_construction_;
        ef_search_ = other.ef_search_;
        space_type_ = std::move(other.space_type_);
        index_ = std::move(other.index_);
        space_ = std::move(other.space_);
        documents_ = std::move(other.documents_);
        internal_id_to_doc_id_ = std::move(other.internal_id_to_doc_id_);
        next_internal_id_ = other.next_internal_id_;
    }
    return *this;
}

void HNSWIndex::initialize_index() {
    // Create space based on type
    if (space_type_ == "l2") {
        space_ = std::make_unique<hnswlib::L2Space>(dim_);
    } else if (space_type_ == "ip") {
        space_ = std::make_unique<hnswlib::InnerProductSpace>(dim_);
    } else {
        throw std::invalid_argument("Invalid space type: " + space_type_ + 
                                   " (supported: 'l2', 'ip')");
    }
    
    // Create HNSWlib index
    index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
        space_.get(), max_elements_, M_, ef_construction_);
    
    // Set default ef for search
    index_->setEf(ef_search_);
}

void HNSWIndex::normalize_vector(std::vector<float>& vec) const {
    // Compute L2 norm
    float norm = 0.0f;
    for (float val : vec) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    
    // Normalize
    if (norm > 1e-10f) {
        for (float& val : vec) {
            val /= norm;
        }
    }
}

float HNSWIndex::ip_to_similarity(float ip_distance) const {
    // HNSWlib returns negative inner product as distance
    // For normalized vectors, cosine similarity = inner product
    // Convert to [0, 1] range where 1 is most similar
    return (-ip_distance + 1.0f) / 2.0f;
}

bool HNSWIndex::add_document(const std::string& doc_id,
                            const std::vector<float>& embedding,
                            const std::string& content,
                            const nlohmann::json& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if document already exists
    if (documents_.find(doc_id) != documents_.end()) {
        return false;  // Document ID already exists
    }
    
    // Validate embedding dimension
    if (embedding.size() != dim_) {
        throw std::invalid_argument("Embedding dimension mismatch: expected " + 
                                   std::to_string(dim_) + ", got " + 
                                   std::to_string(embedding.size()));
    }
    
    // Check capacity
    if (next_internal_id_ >= max_elements_) {
        throw std::runtime_error("Index is full (max_elements: " + 
                                std::to_string(max_elements_) + ")");
    }
    
    // Normalize vector for cosine similarity (if using IP space)
    std::vector<float> normalized_embedding = embedding;
    if (space_type_ == "ip") {
        normalize_vector(normalized_embedding);
    }
    
    // Add to HNSWlib index
    size_t internal_id = next_internal_id_++;
    index_->addPoint(normalized_embedding.data(), internal_id);
    
    // Store metadata
    documents_[doc_id] = DocumentMetadata(doc_id, content, metadata, internal_id);
    internal_id_to_doc_id_[internal_id] = doc_id;
    
    return true;
}

std::vector<SearchResult> HNSWIndex::search(const std::vector<float>& query,
                                           size_t top_k) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate query dimension
    if (query.size() != dim_) {
        throw std::invalid_argument("Query dimension mismatch: expected " + 
                                   std::to_string(dim_) + ", got " + 
                                   std::to_string(query.size()));
    }
    
    // Empty index returns empty results
    if (next_internal_id_ == 0) {
        return {};
    }
    
    // Normalize query for cosine similarity
    std::vector<float> normalized_query = query;
    if (space_type_ == "ip") {
        normalize_vector(normalized_query);
    }
    
    // Limit top_k to available documents
    size_t actual_k = std::min(top_k, static_cast<size_t>(next_internal_id_));
    
    // Search HNSWlib index
    auto result = index_->searchKnn(normalized_query.data(), actual_k);
    
    // Convert results
    std::vector<SearchResult> search_results;
    search_results.reserve(result.size());
    
    while (!result.empty()) {
        auto& pair = result.top();
        float distance = pair.first;
        size_t internal_id = pair.second;
        
        // Convert distance to similarity
        float similarity = (space_type_ == "ip") ? 
            ip_to_similarity(distance) : (1.0f / (1.0f + distance));
        
        // Get document metadata
        auto doc_id_it = internal_id_to_doc_id_.find(internal_id);
        if (doc_id_it != internal_id_to_doc_id_.end()) {
            const auto& doc = documents_[doc_id_it->second];
            search_results.emplace_back(
                doc.doc_id,
                doc.content,
                similarity,
                doc.metadata
            );
        }
        
        result.pop();
    }
    
    // Results are in reverse order (lowest distance first from priority queue)
    // Reverse to get highest similarity first
    std::reverse(search_results.begin(), search_results.end());
    
    return search_results;
}

bool HNSWIndex::remove_document(const std::string& doc_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(doc_id);
    if (it == documents_.end()) {
        return false;  // Document not found
    }
    
    size_t internal_id = it->second.internal_id;
    
    // Mark as deleted in HNSWlib (soft delete)
    index_->markDelete(internal_id);
    
    // Remove from metadata storage
    internal_id_to_doc_id_.erase(internal_id);
    documents_.erase(it);
    
    return true;
}

bool HNSWIndex::has_document(const std::string& doc_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return documents_.find(doc_id) != documents_.end();
}

DocumentMetadata HNSWIndex::get_document(const std::string& doc_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(doc_id);
    if (it != documents_.end()) {
        return it->second;
    }
    
    return DocumentMetadata();  // Return empty metadata if not found
}

bool HNSWIndex::save(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Save HNSWlib index
        index_->saveIndex(filepath);
        
        // Save metadata to separate JSON file
        std::string metadata_path = filepath + ".meta";
        std::ofstream meta_file(metadata_path);
        
        nlohmann::json meta;
        meta["dim"] = dim_;
        meta["max_elements"] = max_elements_;
        meta["M"] = M_;
        meta["ef_construction"] = ef_construction_;
        meta["ef_search"] = ef_search_;
        meta["space_type"] = space_type_;
        meta["next_internal_id"] = next_internal_id_;
        
        // Serialize documents
        nlohmann::json docs_array = nlohmann::json::array();
        for (const auto& [doc_id, doc] : documents_) {
            nlohmann::json doc_json;
            doc_json["doc_id"] = doc.doc_id;
            doc_json["content"] = doc.content;
            doc_json["metadata"] = doc.metadata;
            doc_json["internal_id"] = doc.internal_id;
            docs_array.push_back(doc_json);
        }
        meta["documents"] = docs_array;
        
        meta_file << meta.dump(2);
        meta_file.close();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool HNSWIndex::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Load metadata first
        std::string metadata_path = filepath + ".meta";
        std::ifstream meta_file(metadata_path);
        if (!meta_file.is_open()) {
            return false;
        }
        
        nlohmann::json meta;
        meta_file >> meta;
        meta_file.close();
        
        // Validate and restore configuration
        dim_ = meta["dim"];
        max_elements_ = meta["max_elements"];
        M_ = meta["M"];
        ef_construction_ = meta["ef_construction"];
        ef_search_ = meta["ef_search"];
        space_type_ = meta["space_type"];
        next_internal_id_ = meta["next_internal_id"];
        
        // Recreate space and index
        initialize_index();
        
        // Load HNSWlib index
        index_->loadIndex(filepath, space_.get(), max_elements_);
        index_->setEf(ef_search_);
        
        // Restore documents
        documents_.clear();
        internal_id_to_doc_id_.clear();
        
        for (const auto& doc_json : meta["documents"]) {
            std::string doc_id = doc_json["doc_id"];
            std::string content = doc_json["content"];
            nlohmann::json metadata = doc_json["metadata"];
            size_t internal_id = doc_json["internal_id"];
            
            documents_[doc_id] = DocumentMetadata(doc_id, content, metadata, internal_id);
            internal_id_to_doc_id_[internal_id] = doc_id;
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void HNSWIndex::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Recreate index
    initialize_index();
    
    // Clear metadata
    documents_.clear();
    internal_id_to_doc_id_.clear();
    next_internal_id_ = 0;
}

size_t HNSWIndex::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return documents_.size();
}

IndexStatistics HNSWIndex::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    IndexStatistics stats;
    stats.total_documents = documents_.size();
    stats.dimension = dim_;
    stats.max_elements = max_elements_;
    stats.current_elements = next_internal_id_;
    stats.M = M_;
    stats.ef_construction = ef_construction_;
    stats.ef_search = ef_search_;
    
    // Estimate memory usage (rough approximation)
    // HNSWlib memory: ~(M * 2 * log(N) * dim * sizeof(float)) per element
    if (next_internal_id_ > 0) {
        double log_n = std::log2(static_cast<double>(next_internal_id_));
        double hnsw_memory = next_internal_id_ * M_ * 2 * log_n * dim_ * sizeof(float);
        double metadata_memory = documents_.size() * 1024;  // Rough estimate
        stats.memory_usage_mb = (hnsw_memory + metadata_memory) / (1024.0 * 1024.0);
    }
    
    return stats;
}

void HNSWIndex::set_ef_search(size_t ef) {
    std::lock_guard<std::mutex> lock(mutex_);
    ef_search_ = ef;
    index_->setEf(ef);
}

size_t HNSWIndex::get_ef_search() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ef_search_;
}

} // namespace vector_search
} // namespace brain_ai
