#include "document/ocr_client.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <thread>
#include <cstring>
#include <iostream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <chrono>

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

namespace {
struct ParsedUrl {
    std::string scheme;
    std::string host;
    int port{ -1 };
    std::string path;
};

std::string to_lower(const std::string& input) {
    std::string output = input;
    std::transform(output.begin(), output.end(), output.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return output;
}

ParsedUrl parse_url(const std::string& url) {
    static const std::regex url_regex(R"((https?)://([^/:]+)(?::(\d+))?(\/.*)?$)", std::regex::icase);
    std::smatch match;
    if (!std::regex_match(url, match, url_regex)) {
        throw std::runtime_error("Invalid OCR service URL: " + url);
    }

    ParsedUrl parsed;
    parsed.scheme = to_lower(match[1].str());
    parsed.host = match[2].str();

    if (match[3].matched) {
        parsed.port = std::stoi(match[3].str());
    }

    parsed.path = match[4].matched ? match[4].str() : std::string("/");
    return parsed;
}

bool is_valid_host(const std::string& host) {
    if (host.empty()) {
        return false;
    }
    return std::all_of(host.begin(), host.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '.';
    });
}

bool host_allowed(const std::string& host, const std::vector<std::string>& allow_list) {
    const std::string host_lower = to_lower(host);
    for (const auto& pattern : allow_list) {
        const std::string pattern_lower = to_lower(pattern);
        if (pattern_lower == host_lower) {
            return true;
        }

        const auto wildcard = pattern_lower.find('*');
        if (wildcard != std::string::npos) {
            const std::string prefix = pattern_lower.substr(0, wildcard);
            const std::string suffix = pattern_lower.substr(wildcard + 1);
            const bool prefix_ok = prefix.empty() || host_lower.rfind(prefix, 0) == 0;
            const bool suffix_ok = suffix.empty() || (host_lower.size() >= suffix.size() &&
                host_lower.compare(host_lower.size() - suffix.size(), suffix.size(), suffix) == 0);
            if (prefix_ok && suffix_ok) {
                return true;
            }
        }
    }
    return false;
}

std::string sanitize_path(const std::string& raw_path) {
    if (raw_path.empty() || raw_path == "/") {
        return std::string();
    }
    if (raw_path.find("..") != std::string::npos) {
        throw std::runtime_error("OCR service path must not contain '..'");
    }
    std::string path = raw_path;
    // Remove trailing slash for consistency
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

void apply_timeout(httplib::Client& client, const OCRConfig& config) {
    const auto safe_duration = [](std::chrono::milliseconds duration, std::chrono::milliseconds fallback) {
        return duration.count() <= 0 ? fallback : duration;
    };

    const auto connect_timeout = safe_duration(config.connect_timeout, std::chrono::milliseconds(1000));
    const auto read_timeout = safe_duration(config.read_timeout, std::chrono::milliseconds(5000));
    const auto write_timeout = safe_duration(config.write_timeout, std::chrono::milliseconds(5000));

    const auto apply = [](auto setter, std::chrono::milliseconds duration) {
        auto seconds = static_cast<time_t>(duration.count() / 1000);
        auto micros = static_cast<time_t>((duration.count() % 1000) * 1000);
        setter(seconds, micros);
    };

    apply([&](time_t sec, time_t usec) { client.set_connection_timeout(sec, usec); }, connect_timeout);
    apply([&](time_t sec, time_t usec) { client.set_read_timeout(sec, usec); }, read_timeout);
    apply([&](time_t sec, time_t usec) { client.set_write_timeout(sec, usec); }, write_timeout);
}

} // namespace

// PIMPL implementation details
struct OCRClient::Impl {
    std::unique_ptr<httplib::Client> http_client;
    std::string base_path;
    std::string host;
    int port{0};
    std::string scheme;
    
    explicit Impl(const OCRConfig& config) {
        ParsedUrl parsed = parse_url(config.service_url);
        scheme = parsed.scheme;
        host = parsed.host;
        port = parsed.port;

        if (!is_valid_host(host)) {
            throw std::runtime_error("OCR service host contains invalid characters: " + host);
        }

        if (!host_allowed(host, config.allowed_hosts)) {
            throw std::runtime_error("OCR service host not allowed: " + host);
        }

        if (port < 0) {
            port = (scheme == "https") ? 443 : 80;
        }

        if (port <= 0 || port > 65535) {
            throw std::runtime_error("OCR service port out of range: " + std::to_string(port));
        }

        base_path = sanitize_path(parsed.path);

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        if (scheme == "https") {
            auto ssl_client = std::make_unique<httplib::SSLClient>(host.c_str(), port);
            ssl_client->enable_server_certificate_verification(true);
            http_client = std::move(ssl_client);
        } else {
            http_client = std::make_unique<httplib::Client>(host.c_str(), port);
        }
#else
        if (scheme == "https") {
            throw std::runtime_error("HTTPS OCR endpoints require OpenSSL support");
        }
        http_client = std::make_unique<httplib::Client>(host.c_str(), port);
#endif

        http_client->set_keep_alive(true);
        http_client->set_follow_location(true);
        apply_timeout(*http_client, config);

        Logger::info(
            "OCRClient",
            "HTTP client bound to " + scheme + "://" + host + ":" + std::to_string(port) +
                (base_path.empty() ? std::string() : base_path)
        );
    }

    std::string resolve_endpoint(const std::string& endpoint) const {
        std::string sanitized = endpoint;
        if (sanitized.empty()) {
            sanitized = "/";
        }
        if (sanitized.front() != '/') {
            sanitized.insert(sanitized.begin(), '/');
        }
        if (sanitized.find("..") != std::string::npos) {
            throw std::runtime_error("Endpoint must not contain '..'");
        }

        if (base_path.empty()) {
            return sanitized;
        }
        if (sanitized == "/") {
            return base_path;
        }

        if (base_path.back() == '/') {
            return base_path + sanitized.substr(1);
        }
        return base_path + sanitized;
    }
};

OCRClient::OCRClient(const OCRConfig& config)
    : config_(config) {
    try {
        if (config_.timeout != std::chrono::seconds(30)) {
            auto legacy_ms = std::chrono::duration_cast<std::chrono::milliseconds>(config_.timeout);
            if (legacy_ms.count() > 0) {
                config_.connect_timeout = std::min(config_.connect_timeout, legacy_ms);
                config_.read_timeout = legacy_ms;
                config_.write_timeout = legacy_ms;
            }
        }

        pimpl_ = std::make_unique<Impl>(config_);
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
    if (config_.timeout != std::chrono::seconds(30)) {
        auto legacy_ms = std::chrono::duration_cast<std::chrono::milliseconds>(config_.timeout);
        if (legacy_ms.count() > 0) {
            config_.connect_timeout = std::min(config_.connect_timeout, legacy_ms);
            config_.read_timeout = legacy_ms;
            config_.write_timeout = legacy_ms;
        }
    }
    
    // Recreate HTTP client if URL changed
    if (pimpl_->http_client) {
        pimpl_ = std::make_unique<Impl>(config_);
    }
    
    Logger::info("OCRClient", "Configuration updated");
}

std::optional<std::string> OCRClient::make_request(const std::string& endpoint,
                                                   const std::string& body,
                                                   const std::string& content_type) {
    int attempt = 0;
    
    while (attempt < config_.max_retries) {
        try {
            const auto full_endpoint = pimpl_->resolve_endpoint(endpoint);
            auto response = pimpl_->http_client->Post(full_endpoint.c_str(), body, content_type.c_str());
            
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
