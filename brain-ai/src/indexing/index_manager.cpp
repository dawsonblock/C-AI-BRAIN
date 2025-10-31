#include "indexing/index_manager.hpp"
#include <algorithm>
#include <execution>
#include <fstream>
#include <filesystem>

namespace brain_ai::indexing {

IndexManager::IndexManager(const IndexConfig& config)
    : config_(config),
      last_save_(std::chrono::steady_clock::now()) {
    
    // Initialize HNSW index
    index_ = std::make_unique<vector_search::HNSWIndex>(
        config_.embedding_dim,
        config_.max_elements,
        config_.M,
        config_.ef_construction
    );
    
    index_->set_ef_search(config_.ef_search);
    
    // Load index if path specified and exists
    if (!config_.index_path.empty() && std::filesystem::exists(config_.index_path)) {
        load();
    }
}

IndexManager::~IndexManager() {
    if (config_.auto_save && !config_.index_path.empty()) {
        save();
    }
}

bool IndexManager::add_document(const std::string& doc_id,
                               const std::vector<float>& embedding,
                               const std::string& content,
                               const nlohmann::json& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create full metadata
    auto full_metadata = create_metadata(doc_id, content, metadata);
    
    // Add to index
    if (!index_->add_document(doc_id, embedding, content, full_metadata)) {
        return false;
    }
    
    // Store metadata
    documents_[doc_id] = full_metadata;
    
    // Update stats
    update_stats();
    
    // Auto-save if needed
    if (should_auto_save()) {
        save();
    }
    
    return true;
}

BatchResult IndexManager::add_batch(const std::vector<std::string>& doc_ids,
                                   const std::vector<std::vector<float>>& embeddings,
                                   const std::vector<std::string>& contents,
                                   const std::vector<nlohmann::json>& metadatas) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = doc_ids.size();
    
    // Validate input sizes
    if (doc_ids.size() != embeddings.size() || doc_ids.size() != contents.size()) {
        result.error_messages.push_back("Input size mismatch");
        return result;
    }
    
    // Use metadata if provided, otherwise use empty
    bool has_metadata = !metadatas.empty();
    if (has_metadata && metadatas.size() != doc_ids.size()) {
        result.error_messages.push_back("Metadata size mismatch");
        return result;
    }
    
    // Process in parallel batches
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        try {
            auto metadata = has_metadata ? metadatas[i] : nlohmann::json{};
            auto full_metadata = create_metadata(doc_ids[i], contents[i], metadata);
            
            if (index_->add_document(doc_ids[i], embeddings[i], contents[i], full_metadata)) {
                documents_[doc_ids[i]] = full_metadata;
                result.successful++;
            } else {
                result.failed++;
                result.error_messages.push_back("Failed to add document: " + doc_ids[i]);
            }
        } catch (const std::exception& e) {
            result.failed++;
            result.error_messages.push_back("Exception for " + doc_ids[i] + ": " + e.what());
        }
    }
    
    // Update stats
    update_stats();
    
    // Calculate time
    auto end = std::chrono::steady_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Auto-save if needed
    if (should_auto_save()) {
        save();
    }
    
    return result;
}

std::vector<vector_search::SearchResult> IndexManager::search(
    const std::vector<float>& query_embedding,
    size_t top_k,
    float similarity_threshold) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto results = index_->search(query_embedding, top_k);
    
    // Filter by similarity threshold
    if (similarity_threshold > 0.0f) {
        results.erase(
            std::remove_if(results.begin(), results.end(),
                [similarity_threshold](const auto& r) {
                    return r.similarity < similarity_threshold;
                }),
            results.end()
        );
    }
    
    return results;
}

std::vector<std::vector<vector_search::SearchResult>> IndexManager::search_batch(
    const std::vector<std::vector<float>>& query_embeddings,
    size_t top_k) {
    
    std::vector<std::vector<vector_search::SearchResult>> results;
    results.reserve(query_embeddings.size());
    
    // Process each query
    for (const auto& query : query_embeddings) {
        results.push_back(search(query, top_k));
    }
    
    return results;
}

bool IndexManager::delete_document(const std::string& doc_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remove from metadata
    auto it = documents_.find(doc_id);
    if (it == documents_.end()) {
        return false;
    }
    
    documents_.erase(it);
    
    // Note: HNSWlib doesn't support deletion, so we just remove metadata
    // In production, you'd need to rebuild the index or use a different approach
    
    update_stats();
    
    if (should_auto_save()) {
        save();
    }
    
    return true;
}

bool IndexManager::update_document(const std::string& doc_id,
                                  const std::vector<float>& embedding,
                                  const std::string& content,
                                  const nlohmann::json& metadata) {
    // For HNSWlib, update = delete + add
    delete_document(doc_id);
    return add_document(doc_id, embedding, content, metadata);
}

nlohmann::json IndexManager::get_document(const std::string& doc_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = documents_.find(doc_id);
    if (it != documents_.end()) {
        return it->second;
    }
    
    return nlohmann::json{};
}

bool IndexManager::has_document(const std::string& doc_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return documents_.find(doc_id) != documents_.end();
}

size_t IndexManager::document_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return documents_.size();
}

bool IndexManager::save() {
    if (config_.index_path.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create directory if it doesn't exist
        std::filesystem::path index_path(config_.index_path);
        std::filesystem::create_directories(index_path.parent_path());
        
        // Save index
        if (!index_->save(config_.index_path)) {
            return false;
        }
        
        // Save metadata
        std::string metadata_path = config_.index_path + ".metadata.json";
        nlohmann::json metadata_json;
        for (const auto& [doc_id, metadata] : documents_) {
            metadata_json[doc_id] = metadata;
        }
        
        std::ofstream ofs(metadata_path);
        if (!ofs) {
            return false;
        }
        
        ofs << metadata_json.dump(2);
        
        // Update last save time
        last_save_ = std::chrono::steady_clock::now();
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

bool IndexManager::load() {
    if (config_.index_path.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Load index
        if (!index_->load(config_.index_path)) {
            return false;
        }
        
        // Load metadata
        std::string metadata_path = config_.index_path + ".metadata.json";
        if (!std::filesystem::exists(metadata_path)) {
            return false;
        }
        
        std::ifstream ifs(metadata_path);
        if (!ifs) {
            return false;
        }
        
        nlohmann::json metadata_json;
        ifs >> metadata_json;
        
        // Restore metadata
        documents_.clear();
        for (auto& [doc_id, metadata] : metadata_json.items()) {
            documents_[doc_id] = metadata;
        }
        
        // Update stats
        update_stats();
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

void IndexManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    documents_.clear();
    
    // Re-create index
    index_ = std::make_unique<vector_search::HNSWIndex>(
        config_.embedding_dim,
        config_.max_elements,
        config_.M,
        config_.ef_construction
    );
    
    index_->set_ef_search(config_.ef_search);
    
    update_stats();
}

IndexStats IndexManager::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void IndexManager::set_ef_search(size_t ef_search) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.ef_search = ef_search;
    index_->set_ef_search(ef_search);
}

bool IndexManager::should_auto_save() const {
    if (!config_.auto_save || config_.index_path.empty()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_save_);
    
    return elapsed >= config_.save_interval;
}

void IndexManager::update_stats() {
    stats_.total_documents = documents_.size();
    stats_.total_vectors = index_->size();
    stats_.last_update = std::chrono::system_clock::now();
    
    // Estimate index size (rough approximation)
    stats_.index_size_bytes = stats_.total_vectors * config_.embedding_dim * sizeof(float);
}

nlohmann::json IndexManager::create_metadata(const std::string& doc_id,
                                             const std::string& content,
                                             const nlohmann::json& user_metadata) const {
    nlohmann::json metadata = user_metadata;
    
    // Add system metadata
    metadata["doc_id"] = doc_id;
    metadata["content"] = content;
    metadata["content_length"] = content.length();
    metadata["indexed_at"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    return metadata;
}

} // namespace brain_ai::indexing
