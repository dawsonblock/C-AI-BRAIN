#pragma once

#include "cognitive_handler.hpp"
#include "document/document_processor.hpp"
#include <memory>
#include <atomic>
#include <chrono>

// Forward declarations for gRPC
namespace grpc {
    class Server;
    class ServerBuilder;
    class ServerContext;
    template<typename T> class Status;
}

namespace brain_ai {
namespace proto {
    // Forward declarations for protobuf messages
    class QueryRequest;
    class QueryResponse;
    class BatchQueryRequest;
    class BatchQueryResponse;
    class DocumentRequest;
    class DocumentResponse;
    class BatchDocumentRequest;
    class SearchRequest;
    class SearchResponse;
    class IndexRequest;
    class IndexResponse;
    class EpisodeRequest;
    class EpisodeResponse;
    class RecentEpisodesRequest;
    class SearchEpisodesRequest;
    class EpisodesResponse;
    class HealthCheckRequest;
    class HealthCheckResponse;
    class StatsRequest;
    class StatsResponse;
}
}

namespace brain_ai::grpc_service {

/**
 * @brief Statistics for gRPC service
 */
struct ServiceStats {
    std::atomic<uint64_t> total_queries{0};
    std::atomic<uint64_t> total_documents{0};
    std::atomic<uint64_t> successful_queries{0};
    std::atomic<uint64_t> failed_queries{0};
    std::atomic<uint64_t> successful_documents{0};
    std::atomic<uint64_t> failed_documents{0};
    std::chrono::steady_clock::time_point start_time;
    
    ServiceStats() : start_time(std::chrono::steady_clock::now()) {}
    
    uint64_t uptime_seconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time).count();
    }
};

/**
 * @brief Configuration for gRPC service
 */
struct ServiceConfig {
    std::string server_address = "0.0.0.0:50051";
    int max_concurrent_streams = 100;
    int keepalive_time_ms = 10000;
    int keepalive_timeout_ms = 5000;
    bool enable_reflection = true;
    
    // Cognitive handler config
    size_t episodic_capacity = 1000;
    
    // Document processor config
    document::DocumentProcessor::Config document_config;
    
    ServiceConfig() = default;
};

/**
 * @brief gRPC service implementation for Brain-AI
 * 
 * Provides remote procedure call interface to the Brain-AI cognitive system.
 * Supports query processing, document indexing, vector search, and memory management.
 * 
 * Thread-safe: All methods handle concurrent requests safely.
 * 
 * Example usage:
 * @code
 *   ServiceConfig config;
 *   config.server_address = "0.0.0.0:50051";
 *   config.episodic_capacity = 1000;
 *   
 *   BrainAIServiceImpl service(config);
 *   
 *   service.start();
 *   
 *   // Service now accepting requests...
 *   
 *   service.stop();
 * @endcode
 */
class BrainAIServiceImpl {
public:
    /**
     * @brief Construct service with configuration
     * @param config Service configuration
     */
    explicit BrainAIServiceImpl(const ServiceConfig& config);
    
    /**
     * @brief Destructor - stops server if running
     */
    ~BrainAIServiceImpl();
    
    // Non-copyable and non-movable
    BrainAIServiceImpl(const BrainAIServiceImpl&) = delete;
    BrainAIServiceImpl& operator=(const BrainAIServiceImpl&) = delete;
    BrainAIServiceImpl(BrainAIServiceImpl&&) = delete;
    BrainAIServiceImpl& operator=(BrainAIServiceImpl&&) = delete;
    
    /**
     * @brief Start the gRPC server
     * @return true if server started successfully
     */
    bool start();
    
    /**
     * @brief Stop the gRPC server
     */
    void stop();
    
    /**
     * @brief Wait for server to shutdown
     */
    void wait();
    
    /**
     * @brief Check if server is running
     * @return true if server is running
     */
    bool is_running() const { return running_.load(); }
    
    /**
     * @brief Get service statistics
     * @return Current service stats
     */
    ServiceStats get_stats() const { return stats_; }
    
    /**
     * @brief Get server address
     * @return Server address string
     */
    std::string get_address() const { return config_.server_address; }

private:
    ServiceConfig config_;
    std::unique_ptr<CognitiveHandler> cognitive_;
    std::unique_ptr<document::DocumentProcessor> doc_processor_;
    std::unique_ptr<::grpc::Server> server_;
    std::atomic<bool> running_{false};
    ServiceStats stats_;
    
    // gRPC method implementations (to be implemented with protobuf)
    // These will be implemented in the .cpp file once protobuf is compiled
    
    // Helper methods
    void update_query_stats(bool success);
    void update_document_stats(bool success);
};

/**
 * @brief Builder for gRPC service
 * 
 * Provides fluent interface for configuring and creating the service.
 */
class ServiceBuilder {
public:
    ServiceBuilder() = default;
    
    ServiceBuilder& with_address(const std::string& address) {
        config_.server_address = address;
        return *this;
    }
    
    ServiceBuilder& with_max_streams(int max_streams) {
        config_.max_concurrent_streams = max_streams;
        return *this;
    }
    
    ServiceBuilder& with_episodic_capacity(size_t capacity) {
        config_.episodic_capacity = capacity;
        return *this;
    }
    
    ServiceBuilder& with_ocr_service(const std::string& url) {
        config_.document_config.ocr_config.service_url = url;
        return *this;
    }
    
    ServiceBuilder& enable_reflection(bool enable = true) {
        config_.enable_reflection = enable;
        return *this;
    }
    
    std::unique_ptr<BrainAIServiceImpl> build() {
        return std::make_unique<BrainAIServiceImpl>(config_);
    }

private:
    ServiceConfig config_;
};

} // namespace brain_ai::grpc_service
