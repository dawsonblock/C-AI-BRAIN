#include "document/ocr_client.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <thread>
#include <cstring>
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

// cpp-httplib for HTTP client (will be fetched by CMake)
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

namespace brain_ai::document {

// PIMPL implementation details
struct OCRClient::Impl {
    std::unique_ptr<httplib::Client> http_client;
    
    explicit Impl(const std::string& base_url) {
        // Parse URL
        auto scheme_end = base_url.find("://");
        if (scheme_end == std::string::npos) {
            throw std::runtime_error("Invalid URL: missing scheme");
        }
        
        std::string scheme = base_url.substr(0, scheme_end);
        std::string host_port = base_url.substr(scheme_end + 3);
        
        // Remove trailing slash
        if (!host_port.empty() && host_port.back() == '/') {
            host_port.pop_back();
        }
        
        http_client = std::make_unique<httplib::Client>(base_url);
        http_client->set_keep_alive(true);
    }
};

OCRClient::OCRClient(const OCRConfig& config)
    : config_(config) {
    try {
        pimpl_ = std::make_unique<Impl>(config_.service_url);
        pimpl_->http_client->set_connection_timeout(config_.timeout.count());
        pimpl_->http_client->set_read_timeout(config_.timeout.count());
        pimpl_->http_client->set_write_timeout(config_.timeout.count());
        
        Logger::info("OCRClient", "Initialized with service URL: " + config_.service_url);
    } catch (const std::exception& e) {
        Logger::error("OCRClient", "Failed to initialize: " + std::string(e.what()));
        throw;
    }
}

OCRClient::~OCRClient() = default;

OCRClient::OCRClient(OCRClient&&) noexcept = default;
OCRClient& OCRClient::operator=(OCRClient&&) noexcept = default;

OCRResult OCRClient::process_file(const std::string& filepath) {
    Logger::info("OCRClient", "Processing file: " + filepath);
    
    // Read file into memory
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        OCRResult result;
        result.success = false;
        result.error_message = "Failed to open file: " + filepath;
        Logger::error("OCRClient", result.error_message);
        return result;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        OCRResult result;
        result.success = false;
        result.error_message = "Failed to read file: " + filepath;
        Logger::error("OCRClient", result.error_message);
        return result;
    }
    
    // Determine MIME type from extension
    std::string mime_type = "application/octet-stream";
    auto dot_pos = filepath.rfind('.');
    if (dot_pos != std::string::npos) {
        std::string ext = filepath.substr(dot_pos + 1);
        if (ext == "png") mime_type = "image/png";
        else if (ext == "jpg" || ext == "jpeg") mime_type = "image/jpeg";
        else if (ext == "pdf") mime_type = "application/pdf";
        else if (ext == "tiff" || ext == "tif") mime_type = "image/tiff";
    }
    
    return process_image(buffer, mime_type);
}

OCRResult OCRClient::process_image(const std::vector<uint8_t>& image_data,
                                   const std::string& mime_type) {
    auto start_time = std::chrono::steady_clock::now();
    
    Logger::info("OCRClient", "Processing image (" + std::to_string(image_data.size()) + 
                 " bytes, type: " + mime_type + ")");
    
    // Create multipart form data
    std::string boundary = generate_boundary();
    std::string body = create_multipart_body(image_data, mime_type, boundary);
    
    std::string content_type = "multipart/form-data; boundary=" + boundary;
    
    // Make request with retries
    auto response = make_request("/ocr/extract", body, content_type);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    if (!response) {
        OCRResult result;
        result.success = false;
        result.error_message = "Failed to get response from OCR service";
        result.processing_time = duration;
        Logger::error("OCRClient", result.error_message);
        return result;
    }
    
    // Parse response
    auto result = parse_response(*response);
    result.processing_time = duration;
    
    Logger::info("OCRClient", "Processing completed in " + 
                std::to_string(duration.count()) + "ms");
    
    return result;
}

std::vector<OCRResult> OCRClient::process_batch(const std::vector<std::string>& filepaths) {
    Logger::info("OCRClient", "Batch processing " + std::to_string(filepaths.size()) + " files");
    
    std::vector<OCRResult> results;
    results.reserve(filepaths.size());
    
    // Process each file sequentially
    // TODO: Could parallelize with thread pool for better performance
    for (const auto& filepath : filepaths) {
        results.push_back(process_file(filepath));
    }
    
    size_t success_count = 0;
    for (const auto& result : results) {
        if (result.success) success_count++;
    }
    
    Logger::info("OCRClient", "Batch completed: " + std::to_string(success_count) + 
                "/" + std::to_string(results.size()) + " succeeded");
    
    return results;
}

bool OCRClient::check_health() {
    try {
        auto response = pimpl_->http_client->Get("/health");
        
        if (!response || response->status != 200) {
            Logger::warn("OCRClient", "Health check failed: status " + 
                        std::to_string(response ? response->status : 0));
            return false;
        }
        
        // Parse response
        auto json = nlohmann::json::parse(response->body, nullptr, false);
        if (json.is_discarded()) {
            Logger::warn("OCRClient", "Health check: invalid JSON response");
            return false;
        }
        
        return json.value("status", "") == "healthy";
        
    } catch (const std::exception& e) {
        Logger::error("OCRClient", "Health check exception: " + std::string(e.what()));
        return false;
    }
}

nlohmann::json OCRClient::get_service_status() {
    try {
        auto response = pimpl_->http_client->Get("/health");
        
        if (!response || response->status != 200) {
            return nlohmann::json::object();
        }
        
        auto json = nlohmann::json::parse(response->body, nullptr, false);
        if (json.is_discarded()) {
            return nlohmann::json::object();
        }
        
        return json;
        
    } catch (const std::exception& e) {
        Logger::error("OCRClient", "Get status exception: " + std::string(e.what()));
        return nlohmann::json::object();
    }
}

void OCRClient::update_config(const OCRConfig& config) {
    config_ = config;
    
    // Recreate HTTP client if URL changed
    if (pimpl_->http_client) {
        pimpl_ = std::make_unique<Impl>(config_.service_url);
        pimpl_->http_client->set_connection_timeout(config_.timeout.count());
        pimpl_->http_client->set_read_timeout(config_.timeout.count());
        pimpl_->http_client->set_write_timeout(config_.timeout.count());
    }
    
    Logger::info("OCRClient", "Configuration updated");
}

std::optional<std::string> OCRClient::make_request(const std::string& endpoint,
                                                   const std::string& body,
                                                   const std::string& content_type) {
    int attempt = 0;
    
    while (attempt < config_.max_retries) {
        try {
            auto response = pimpl_->http_client->Post(endpoint.c_str(), body, content_type.c_str());
            
            if (!response) {
                Logger::warn("OCRClient", "Request failed: no response (attempt " + 
                           std::to_string(attempt + 1) + ")");
                attempt++;
                if (attempt < config_.max_retries) {
                    std::this_thread::sleep_for(config_.retry_delay);
                }
                continue;
            }
            
            if (response->status != 200) {
                Logger::warn("OCRClient", "Request failed: HTTP " + 
                           std::to_string(response->status) + " (attempt " +
                           std::to_string(attempt + 1) + ")");
                attempt++;
                if (attempt < config_.max_retries) {
                    std::this_thread::sleep_for(config_.retry_delay);
                }
                continue;
            }
            
            return response->body;
            
        } catch (const std::exception& e) {
            Logger::error("OCRClient", "Request exception: " + std::string(e.what()) +
                         " (attempt " + std::to_string(attempt + 1) + ")");
            attempt++;
            if (attempt < config_.max_retries) {
                std::this_thread::sleep_for(config_.retry_delay);
            }
        }
    }
    
    return std::nullopt;
}

OCRResult OCRClient::parse_response(const std::string& json_str) {
    OCRResult result;
    
    try {
        auto json = nlohmann::json::parse(json_str);
        
        result.text = json.value("text", "");
        result.confidence = json.value("confidence", 0.0f);
        result.success = json.value("success", false);
        result.error_message = json.value("error_message", "");
        
        if (json.contains("metadata")) {
            result.metadata = json["metadata"];
        }
        
        if (json.contains("processing_time_ms")) {
            result.processing_time = std::chrono::milliseconds(
                json["processing_time_ms"].get<int>());
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Failed to parse response: " + std::string(e.what());
        Logger::error("OCRClient", result.error_message);
    }
    
    return result;
}

std::string OCRClient::create_multipart_body(const std::vector<uint8_t>& image_data,
                                            const std::string& mime_type,
                                            const std::string& boundary) {
    std::ostringstream body;
    
    // Add file field
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"file\"; filename=\"document\"\r\n";
    body << "Content-Type: " << mime_type << "\r\n\r\n";
    body.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
    body << "\r\n";
    
    // Add mode field
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"mode\"\r\n\r\n";
    body << config_.mode << "\r\n";
    
    // Add task field
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"task\"\r\n\r\n";
    body << config_.task << "\r\n";
    
    // Add max_tokens field
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"max_tokens\"\r\n\r\n";
    body << std::to_string(config_.max_tokens) << "\r\n";
    
    // Add temperature field
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"temperature\"\r\n\r\n";
    body << std::to_string(config_.temperature) << "\r\n";
    
    // Final boundary
    body << "--" << boundary << "--\r\n";
    
    return body.str();
}

std::string OCRClient::generate_boundary() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    std::string boundary = "----BrainAIFormBoundary";
    for (int i = 0; i < 16; ++i) {
        boundary += alphanum[dis(gen)];
    }
    
    return boundary;
}

} // namespace brain_ai::document
