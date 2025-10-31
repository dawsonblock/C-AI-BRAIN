#include "document/document_processor.hpp"
#include "utils.hpp"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <iostream>

// Logger placeholder (replace with full logging system if available)
namespace Logger {
    inline void info(const std::string& component, const std::string& message) {
        std::cout << "[INFO] " << component << ": " << message << std::endl;
    }
    inline void warn(const std::string& component, const std::string& message) {
        std::cout << "[WARN] " << component << ": " << message << std::endl;
    }
    inline void error(const std::string& component, const std::string& message) {
        std::cerr << "[ERROR] " << component << ": " << message << std::endl;
    }
}

namespace brain_ai::document {

DocumentProcessor::DocumentProcessor(CognitiveHandler& cognitive_handler,
                                     const Config& config)
    : cognitive_(cognitive_handler)
    , config_(config) {
    
    ocr_client_ = std::make_unique<OCRClient>(config_.ocr_config);
    validator_ = std::make_unique<TextValidator>(config_.validation_config);
    
    Logger::info("DocumentProcessor", "Initialized document processing pipeline");
}

DocumentProcessor::~DocumentProcessor() = default;

DocumentResult DocumentProcessor::process(const std::string& filepath,
                                         const std::string& doc_id) {
    auto start_time = std::chrono::steady_clock::now();
    
    DocumentResult result;
    result.doc_id = doc_id.empty() ? generate_doc_id(filepath) : doc_id;
    
    Logger::info("DocumentProcessor", "Processing document: " + filepath + 
                " (ID: " + result.doc_id + ")");
    
    try {
        // Step 1: OCR extraction
        auto ocr_result = ocr_client_->process_file(filepath);
        if (!ocr_result.success) {
            result.success = false;
            result.error_message = "OCR failed: " + ocr_result.error_message;
            Logger::error("DocumentProcessor", result.error_message);
            update_stats(result);
            return result;
        }
        
        result.extracted_text = ocr_result.text;
        result.ocr_confidence = ocr_result.confidence;
        result.metadata = ocr_result.metadata;
        result.metadata["source_file"] = filepath;
        
        Logger::info("DocumentProcessor", "OCR extracted " + 
                    std::to_string(ocr_result.text.size()) + " chars");
        
        // Step 2: Text validation
        auto validation_result = validator_->validate(ocr_result.text);
        if (!validation_result.is_valid) {
            result.success = false;
            result.error_message = "Validation failed: low confidence";
            result.validation_confidence = validation_result.confidence;
            
            Logger::warn("DocumentProcessor", 
                        "Validation failed: confidence=" + 
                        std::to_string(validation_result.confidence) +
                        ", errors=" + std::to_string(validation_result.errors_corrected));
            
            // Still return the text for inspection
            result.validated_text = validation_result.cleaned_text;
            update_stats(result);
            return result;
        }
        
        result.validated_text = validation_result.cleaned_text;
        result.validation_confidence = validation_result.confidence;
        
        Logger::info("DocumentProcessor", "Text validated: confidence=" + 
                    std::to_string(validation_result.confidence) +
                    ", corrections=" + std::to_string(validation_result.errors_corrected));
        
        // Step 3: Generate embedding (if configured)
        std::vector<float> embedding;
        if (config_.auto_generate_embeddings) {
            embedding = generate_embedding(result.validated_text);
            Logger::info("DocumentProcessor", "Generated embedding: " + 
                        std::to_string(embedding.size()) + " dimensions");
        }
        
        // Step 4: Create episodic memory (if configured)
        if (config_.create_episodic_memory) {
            if (!create_memory(result.doc_id, result.validated_text, result.metadata)) {
                Logger::warn("DocumentProcessor", "Failed to create episodic memory");
            } else {
                Logger::info("DocumentProcessor", "Created episodic memory");
            }
        }
        
        // Step 5: Index in vector store (if configured)
        if (config_.index_in_vector_store && !embedding.empty()) {
            result.indexed = index_document(result.doc_id, embedding,
                                           result.validated_text, result.metadata);
            
            if (result.indexed) {
                Logger::info("DocumentProcessor", "Indexed in vector store");
            } else {
                Logger::warn("DocumentProcessor", "Failed to index in vector store");
            }
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Processing exception: " + std::string(e.what());
        Logger::error("DocumentProcessor", result.error_message);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    Logger::info("DocumentProcessor", "Processing completed in " + 
                std::to_string(result.processing_time.count()) + "ms");
    
    update_stats(result);
    
    return result;
}

DocumentResult DocumentProcessor::process_image(const std::vector<uint8_t>& image_data,
                                               const std::string& mime_type,
                                               const std::string& doc_id) {
    auto start_time = std::chrono::steady_clock::now();
    
    DocumentResult result;
    result.doc_id = doc_id;
    
    Logger::info("DocumentProcessor", "Processing image: " + doc_id);
    
    try {
        // Step 1: OCR extraction
        auto ocr_result = ocr_client_->process_image(image_data, mime_type);
        if (!ocr_result.success) {
            result.success = false;
            result.error_message = "OCR failed: " + ocr_result.error_message;
            Logger::error("DocumentProcessor", result.error_message);
            update_stats(result);
            return result;
        }
        
        result.extracted_text = ocr_result.text;
        result.ocr_confidence = ocr_result.confidence;
        result.metadata = ocr_result.metadata;
        
        // Step 2: Validation
        auto validation_result = validator_->validate(ocr_result.text);
        result.validated_text = validation_result.cleaned_text;
        result.validation_confidence = validation_result.confidence;
        
        if (!validation_result.is_valid) {
            result.success = false;
            result.error_message = "Validation failed";
            update_stats(result);
            return result;
        }
        
        // Step 3-5: Same as process()
        std::vector<float> embedding;
        if (config_.auto_generate_embeddings) {
            embedding = generate_embedding(result.validated_text);
        }
        
        if (config_.create_episodic_memory) {
            create_memory(result.doc_id, result.validated_text, result.metadata);
        }
        
        if (config_.index_in_vector_store && !embedding.empty()) {
            result.indexed = index_document(result.doc_id, embedding,
                                           result.validated_text, result.metadata);
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Processing exception: " + std::string(e.what());
        Logger::error("DocumentProcessor", result.error_message);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    update_stats(result);
    
    return result;
}

std::vector<DocumentResult> DocumentProcessor::process_batch(
    const std::vector<std::string>& filepaths,
    ProgressCallback progress_callback) {
    
    Logger::info("DocumentProcessor", "Batch processing " + 
                std::to_string(filepaths.size()) + " documents");
    
    std::vector<DocumentResult> results;
    results.reserve(filepaths.size());
    
    size_t current = 0;
    for (const auto& filepath : filepaths) {
        current++;
        
        if (progress_callback) {
            progress_callback(current, filepaths.size(), "Processing: " + filepath);
        }
        
        auto result = process(filepath);
        results.push_back(std::move(result));
    }
    
    // Summary
    size_t success_count = std::count_if(results.begin(), results.end(),
                                        [](const auto& r) { return r.success; });
    
    Logger::info("DocumentProcessor", "Batch completed: " + 
                std::to_string(success_count) + "/" + 
                std::to_string(results.size()) + " succeeded");
    
    return results;
}

DocumentResult DocumentProcessor::process_with_embedding(
    const std::string& filepath,
    const std::vector<float>& embedding,
    const std::string& doc_id) {
    
    // Disable auto-embedding for this call
    bool prev_auto = config_.auto_generate_embeddings;
    config_.auto_generate_embeddings = false;
    
    auto result = process(filepath, doc_id);
    
    // Restore setting
    config_.auto_generate_embeddings = prev_auto;
    
    // Use provided embedding
    if (result.success && config_.index_in_vector_store) {
        result.indexed = index_document(result.doc_id, embedding,
                                       result.validated_text, result.metadata);
    }
    
    return result;
}

ProcessingStats DocumentProcessor::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void DocumentProcessor::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ProcessingStats{};
    Logger::info("DocumentProcessor", "Statistics reset");
}

void DocumentProcessor::update_config(const Config& config) {
    config_ = config;
    
    ocr_client_->update_config(config_.ocr_config);
    validator_->update_config(config_.validation_config);
    
    Logger::info("DocumentProcessor", "Configuration updated");
}

bool DocumentProcessor::check_service_health() {
    bool healthy = ocr_client_->check_health();
    
    if (healthy) {
        Logger::info("DocumentProcessor", "OCR service is healthy");
    } else {
        Logger::warn("DocumentProcessor", "OCR service is unhealthy");
    }
    
    return healthy;
}

std::string DocumentProcessor::generate_doc_id(const std::string& filepath) {
    // Extract filename
    auto last_slash = filepath.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) 
        ? filepath.substr(last_slash + 1) : filepath;
    
    // Generate timestamp-based ID
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    
    std::ostringstream oss;
    oss << "doc_" << filename << "_" << millis;
    
    return oss.str();
}

std::vector<float> DocumentProcessor::generate_embedding(const std::string& text) {
    // TODO: Call external embedding service (OpenAI, HuggingFace, etc.)
    // For now, generate random embedding for testing
    
    Logger::warn("DocumentProcessor", "Using stub embedding generation (random)");
    
    const size_t embedding_dim = 1536;  // OpenAI ada-002 dimension
    std::vector<float> embedding(embedding_dim);
    
    // Use text hash as seed for reproducibility
    std::hash<std::string> hasher;
    size_t seed = hasher(text);
    
    std::mt19937 gen(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    // Generate random normalized vector
    for (auto& val : embedding) {
        val = dist(gen);
    }
    
    // Normalize to unit length
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    
    if (norm > 0.0f) {
        for (auto& val : embedding) {
            val /= norm;
        }
    }
    
    return embedding;
}

bool DocumentProcessor::create_memory(const std::string& doc_id,
                                      const std::string& text,
                                      const nlohmann::json& metadata) {
    try {
        // Create episode in episodic buffer
        std::string query = "Document: " + doc_id;
        std::string response = text.substr(0, std::min<size_t>(text.size(), 1000));  // Truncate if too long
        
        // Create a stub embedding (will be replaced with real embedding in production)
        std::vector<float> stub_embedding(1536, 0.0f);
        
        // Convert JSON metadata to string map
        std::unordered_map<std::string, std::string> meta_map;
        meta_map["doc_id"] = doc_id;
        meta_map["source"] = "document_processor";
        
        cognitive_.episodic_buffer().add_episode(query, response, stub_embedding, meta_map);
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("DocumentProcessor", 
                     "Failed to create memory: " + std::string(e.what()));
        return false;
    }
}

bool DocumentProcessor::index_document(const std::string& doc_id,
                                       const std::vector<float>& embedding,
                                       const std::string& text,
                                       const nlohmann::json& metadata) {
    try {
        return cognitive_.index_document(doc_id, embedding, text, metadata);
        
    } catch (const std::exception& e) {
        Logger::error("DocumentProcessor", 
                     "Failed to index document: " + std::string(e.what()));
        return false;
    }
}

void DocumentProcessor::update_stats(const DocumentResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_documents++;
    if (result.success) {
        stats_.successful++;
    } else {
        stats_.failed++;
    }
    
    stats_.total_time += result.processing_time;
    stats_.update();
}

} // namespace brain_ai::document
