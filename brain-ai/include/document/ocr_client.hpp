#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include "nlohmann/json.hpp"

namespace brain_ai::document {

/**
 * @brief Result from OCR processing
 */
struct OCRResult {
    std::string text;                        // Extracted text content
    float confidence;                        // Overall confidence score [0.0-1.0]
    std::chrono::milliseconds processing_time; // Processing duration
    nlohmann::json metadata;                 // Additional metadata
    bool success;                            // Operation success flag
    std::string error_message;               // Error details if failed
    
    OCRResult() : confidence(0.0f), processing_time(0), success(false) {}
};

/**
 * @brief Configuration for OCR processing
 */
struct OCRConfig {
    std::string service_url = "http://localhost:8000";  // DeepSeek-OCR service URL
    std::string mode = "base";                          // Resolution: tiny|small|base|large|gundam
    std::string task = "markdown";                      // Task: ocr|markdown|figure|reference|describe
    int max_tokens = 8192;                              // Maximum tokens to generate
    float temperature = 0.0f;                           // Sampling temperature
    std::chrono::seconds timeout{30};                   // Request timeout
    int max_retries = 3;                                // Max retry attempts
    std::chrono::milliseconds retry_delay{1000};        // Delay between retries
    
    OCRConfig() = default;
};

/**
 * @brief HTTP client for DeepSeek-OCR service
 * 
 * Provides interface to communicate with the Python OCR service.
 * Handles HTTP multipart/form-data uploads, request/response parsing,
 * error handling, retries, and timeout management.
 * 
 * Thread-safe: Multiple threads can use separate instances safely.
 * 
 * Example usage:
 * @code
 *   OCRConfig config;
 *   config.service_url = "http://localhost:8000";
 *   config.mode = "base";
 *   config.task = "markdown";
 *   
 *   OCRClient client(config);
 *   
 *   // From file
 *   auto result = client.process_file("/path/to/document.pdf");
 *   if (result.success) {
 *       std::cout << "Extracted text: " << result.text << std::endl;
 *   }
 *   
 *   // From image data
 *   std::vector<uint8_t> image_data = load_image();
 *   auto result2 = client.process_image(image_data, "image/png");
 * @endcode
 */
class OCRClient {
public:
    /**
     * @brief Construct OCR client with configuration
     * @param config Client configuration
     */
    explicit OCRClient(const OCRConfig& config = OCRConfig());
    
    /**
     * @brief Destructor
     */
    ~OCRClient();
    
    // Non-copyable but movable
    OCRClient(const OCRClient&) = delete;
    OCRClient& operator=(const OCRClient&) = delete;
    OCRClient(OCRClient&&) noexcept;
    OCRClient& operator=(OCRClient&&) noexcept;
    
    /**
     * @brief Process document from file path
     * @param filepath Path to document file
     * @return OCR processing result
     */
    OCRResult process_file(const std::string& filepath);
    
    /**
     * @brief Process document from image data
     * @param image_data Raw image bytes
     * @param mime_type MIME type (e.g., "image/png", "image/jpeg", "application/pdf")
     * @return OCR processing result
     */
    OCRResult process_image(const std::vector<uint8_t>& image_data,
                           const std::string& mime_type);
    
    /**
     * @brief Process multiple documents in batch
     * @param filepaths Vector of file paths
     * @return Vector of OCR results (same order as input)
     */
    std::vector<OCRResult> process_batch(const std::vector<std::string>& filepaths);
    
    /**
     * @brief Check service health
     * @return true if service is healthy and reachable
     */
    bool check_health();
    
    /**
     * @brief Get service status information
     * @return JSON with status details or empty if unavailable
     */
    nlohmann::json get_service_status();
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void update_config(const OCRConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const OCRConfig& get_config() const { return config_; }

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    OCRConfig config_;
    
    /**
     * @brief Make HTTP POST request with retries
     * @param endpoint API endpoint (e.g., "/ocr/extract")
     * @param body Request body
     * @param content_type Content-Type header
     * @return Response body or empty on failure
     */
    std::optional<std::string> make_request(const std::string& endpoint,
                                           const std::string& body,
                                           const std::string& content_type);
    
    /**
     * @brief Parse OCR response JSON
     * @param json_str JSON response string
     * @return Parsed OCR result
     */
    OCRResult parse_response(const std::string& json_str);
    
    /**
     * @brief Create multipart form data
     * @param image_data Image bytes
     * @param mime_type MIME type
     * @param boundary Multipart boundary string
     * @return Multipart body
     */
    std::string create_multipart_body(const std::vector<uint8_t>& image_data,
                                     const std::string& mime_type,
                                     const std::string& boundary);
    
    /**
     * @brief Generate random boundary string
     * @return Boundary string
     */
    std::string generate_boundary();
};

} // namespace brain_ai::document
