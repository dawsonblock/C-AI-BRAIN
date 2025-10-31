#include "grpc/brain_ai_service.hpp"
#include <iostream>

namespace brain_ai::grpc_service {

BrainAIServiceImpl::BrainAIServiceImpl(const ServiceConfig& config)
    : config_(config) {
    
    // Initialize cognitive handler
    cognitive_ = std::make_unique<CognitiveHandler>(config_.episodic_capacity);
    
    // Initialize document processor
    doc_processor_ = std::make_unique<document::DocumentProcessor>(
        *cognitive_, config_.document_config);
    
    std::cout << "[BrainAIService] Initialized with address: " 
              << config_.server_address << std::endl;
}

BrainAIServiceImpl::~BrainAIServiceImpl() {
    if (running_.load()) {
        stop();
    }
}

bool BrainAIServiceImpl::start() {
    if (running_.load()) {
        std::cerr << "[BrainAIService] Server is already running" << std::endl;
        return false;
    }
    
    try {
        std::cout << "[BrainAIService] Starting server at "
                  << config_.server_address << "..." << std::endl;

        grpc::ServerBuilder builder;
        
        // Add listening port
        builder.AddListeningPort(
            config_.server_address,
            grpc::InsecureServerCredentials()
        );
        
        // Register main service
        builder.RegisterService(this);
        
        // Enable health checking
        health_service_ = std::make_unique<grpc::DefaultHealthCheckService>();
        builder.RegisterService(health_service_.get());
        health_service_->SetServingStatus("", grpc::health::v1::HealthCheckResponse::SERVING);
        
        // Enable reflection for debugging (if enabled)
        if (config_.enable_reflection) {
            grpc::reflection::InitProtoReflectionServerBuilderPlugin();
            std::cout << "[BrainAIService] gRPC reflection enabled" << std::endl;
        }
        
        // Set server limits
        builder.SetMaxReceiveMessageSize(100 * 1024 * 1024);  // 100MB max message
        builder.SetMaxSendMessageSize(100 * 1024 * 1024);
        
        // Build and start server
        server_ = builder.BuildAndStart();
        if (!server_) {
            std::cerr << "[BrainAIService] Failed to build server" << std::endl;
            return false;
        }
        
        running_.store(true);
        std::cout << "[BrainAIService] ✅ Server listening on " 
                  << config_.server_address << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[BrainAIService] Start failed: " << e.what() << std::endl;
        return false;
    }
}

void BrainAIServiceImpl::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[BrainAIService] Stopping server..." << std::endl;
    
    // Update health status
    if (health_service_) {
        health_service_->SetServingStatus("", grpc::health::v1::HealthCheckResponse::NOT_SERVING);
    }
    
    // Shutdown server gracefully
    if (server_) {
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        server_->Shutdown(deadline);
    }
    
    running_.store(false);
    
    std::cout << "[BrainAIService] ✅ Server stopped" << std::endl;
}

void BrainAIServiceImpl::wait() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[BrainAIService] Waiting for server to shutdown..." << std::endl;
    
    if (server_) {
        server_->Wait();
    }
    
    std::cout << "[BrainAIService] Server wait completed" << std::endl;
}

void BrainAIServiceImpl::update_query_stats(bool success) {
    stats_.total_queries.fetch_add(1);
    if (success) {
        stats_.successful_queries.fetch_add(1);
    } else {
        stats_.failed_queries.fetch_add(1);
    }
}

void BrainAIServiceImpl::update_document_stats(bool success) {
    stats_.total_documents.fetch_add(1);
    if (success) {
        stats_.successful_documents.fetch_add(1);
    } else {
        stats_.failed_documents.fetch_add(1);
    }
}

} // namespace brain_ai::grpc_service
