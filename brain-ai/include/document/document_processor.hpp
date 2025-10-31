#pragma once

#include "document/ocr_client.hpp"
#include "document/text_validator.hpp"
#include "cognitive_handler.hpp"
#include <memory>
#include <functional>
#include <chrono>

namespace brain_ai::document {

/**
 * @brief Document processing statistics
 */
struct ProcessingStats {
    size_t total_documents = 0;
    size_t successful = 0;
    size_t failed = 0;
    std::chrono::milliseconds total_time{0};
    std::chrono::milliseconds avg_time{0};
    
    void update() {
        if (total_documents > 0) {
            avg_time = total_time / total_documents;
        }
    }
};

/**
 * @brief Result from document processing pipeline
 */
struct DocumentResult {
    std::string doc_id;                         // Document identifier
    std::string extracted_text;                 // Final extracted text
    std::string validated_text;                 // Validated/cleaned text
    float ocr_confidence;                       // OCR confidence score
    float validation_confidence;                // Validation confidence score
    bool indexed;                               // Successfully indexed in vector store
    bool success;                               // Overall success flag
    std::string error_message;                  // Error details if failed
    std::chrono::milliseconds processing_time;  // Total processing time
    nlohmann::json metadata;                    // Additional metadata
    
    DocumentResult() 
        : ocr_confidence(0.0f)
        , validation_confidence(0.0f)
        , indexed(false)
        , success(false)
        , processing_time(0) {}
};

/**
 * @brief Callback for processing progress updates
 */
using ProgressCallback = std::function<void(size_t current, size_t total, const std::string& status)>;

/**
 * @brief End-to-end document processing pipeline
 * 
 * Orchestrates the complete document processing workflow:
 * 1. Send document to DeepSeek-OCR service
 * 2. Receive OCR-extracted text
 * 3. Validate and clean text
 * 4. Generate embeddings (via external service)
 * 5. Create memory in episodic buffer
 * 6. Index in vector search store
 * 
 * Integrates OCRClient, TextValidator, and CognitiveHandler components.
 * 
 * Thread-safe: Can process documents from multiple threads.
 * 
 * Example usage:
 * @code
 *   CognitiveHandler cognitive(1000);
 *   
 *   DocumentProcessorConfig config;
 *   config.ocr_config.service_url = "http://localhost:8000";
 *   config.ocr_config.mode = "base";
 *   
 *   DocumentProcessor processor(cognitive, config);
 *   
 *   // Single document
 *   auto result = processor.process("/path/to/document.pdf");
 *   if (result.success) {
 *       std::cout << "Indexed: " << result.doc_id << std::endl;
 *   }
 *   
 *   // Batch processing with progress callback
 *   std::vector<std::string> files = {"doc1.pdf", "doc2.png", "doc3.jpg"};
 *   auto results = processor.process_batch(files, [](size_t cur, size_t tot, const std::string& status) {
 *       std::cout << "[" << cur << "/" << tot << "] " << status << std::endl;
 *   });
 * @endcode
 */
class DocumentProcessor {
public:
    /**
     * @brief Configuration for document processor
     */
    struct Config {
        OCRConfig ocr_config;                   // OCR client configuration
        ValidationConfig validation_config;     // Text validator configuration
        bool auto_generate_embeddings = true;   // Auto-generate embeddings
        bool create_episodic_memory = true;     // Create episodic memory
        bool index_in_vector_store = true;      // Index in vector search
        size_t batch_size = 10;                 // Batch size for parallel processing
        
        Config() = default;
    };
    
    /**
     * @brief Construct document processor
     * @param cognitive_handler Reference to cognitive handler for memory/indexing
     * @param config Processor configuration
     */
    explicit DocumentProcessor(CognitiveHandler& cognitive_handler,
                              const Config& config);
    
    /**
     * @brief Destructor
     */
    ~DocumentProcessor();
    
    // Non-copyable and non-movable (contains mutex)
    DocumentProcessor(const DocumentProcessor&) = delete;
    DocumentProcessor& operator=(const DocumentProcessor&) = delete;
    DocumentProcessor(DocumentProcessor&&) = delete;
    DocumentProcessor& operator=(DocumentProcessor&&) = delete;
    
    /**
     * @brief Process single document from file
     * @param filepath Path to document file
     * @param doc_id Optional document ID (auto-generated if empty)
     * @return Processing result
     */
    DocumentResult process(const std::string& filepath,
                          const std::string& doc_id = "");
    
    /**
     * @brief Process document from image data
     * @param image_data Raw image bytes
     * @param mime_type MIME type
     * @param doc_id Document identifier
     * @return Processing result
     */
    DocumentResult process_image(const std::vector<uint8_t>& image_data,
                                 const std::string& mime_type,
                                 const std::string& doc_id);
    
    /**
     * @brief Process multiple documents in batch
     * @param filepaths Vector of file paths
     * @param progress_callback Optional progress callback
     * @return Vector of processing results
     */
    std::vector<DocumentResult> process_batch(
        const std::vector<std::string>& filepaths,
        ProgressCallback progress_callback = nullptr);
    
    /**
     * @brief Process document with custom embedding
     * @param filepath Path to document file
     * @param embedding Pre-computed embedding vector
     * @param doc_id Document identifier
     * @return Processing result
     */
    DocumentResult process_with_embedding(
        const std::string& filepath,
        const std::vector<float>& embedding,
        const std::string& doc_id);
    
    /**
     * @brief Get processing statistics
     * @return Current statistics
     */
    ProcessingStats get_stats() const;
    
    /**
     * @brief Reset statistics counters
     */
    void reset_stats();
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void update_config(const Config& config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const Config& get_config() const { return config_; }
    
    /**
     * @brief Check if OCR service is healthy
     * @return true if service is reachable and healthy
     */
    bool check_service_health();

private:
    CognitiveHandler& cognitive_;
    Config config_;
    std::unique_ptr<OCRClient> ocr_client_;
    std::unique_ptr<TextValidator> validator_;
    
    mutable std::mutex stats_mutex_;
    ProcessingStats stats_;
    
    /**
     * @brief Generate document ID
     * @param filepath Source file path
     * @return Unique document ID
     */
    std::string generate_doc_id(const std::string& filepath);
    
    /**
     * @brief Generate embedding for text (stub - should call external service)
     * @param text Input text
     * @return Embedding vector
     */
    std::vector<float> generate_embedding(const std::string& text);
    
    /**
     * @brief Create episodic memory from document
     * @param doc_id Document ID
     * @param text Document text
     * @param metadata Document metadata
     * @return true if successful
     */
    bool create_memory(const std::string& doc_id,
                      const std::string& text,
                      const nlohmann::json& metadata);
    
    /**
     * @brief Index document in vector store
     * @param doc_id Document ID
     * @param embedding Embedding vector
     * @param text Document text
     * @param metadata Document metadata
     * @return true if successful
     */
    bool index_document(const std::string& doc_id,
                       const std::vector<float>& embedding,
                       const std::string& text,
                       const nlohmann::json& metadata);
    
    /**
     * @brief Update statistics
     * @param result Processing result
     */
    void update_stats(const DocumentResult& result);
};

} // namespace brain_ai::document
