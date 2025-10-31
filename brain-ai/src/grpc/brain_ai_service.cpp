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
    
    std::cout << "[BrainAIService] Starting server at " 
              << config_.server_address << "..." << std::endl;
    
    // TODO: Implement gRPC server startup once protobuf is compiled
    // This would involve:
    // 1. grpc::ServerBuilder builder;
    // 2. builder.AddListeningPort(config_.server_address, grpc::InsecureServerCredentials());
    // 3. builder.RegisterService(this);
    // 4. server_ = builder.BuildAndStart();
    
    running_.store(true);
    
    std::cout << "[BrainAIService] Server started successfully" << std::endl;
    std::cout << "[BrainAIService] Listening on " << config_.server_address << std::endl;
    
    if (config_.enable_reflection) {
        std::cout << "[BrainAIService] gRPC reflection enabled" << std::endl;
    }
    
    return true;
}

void BrainAIServiceImpl::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[BrainAIService] Stopping server..." << std::endl;
    
    // TODO: Implement server shutdown
    // if (server_) {
    //     server_->Shutdown();
    // }
    
    running_.store(false);
    
    std::cout << "[BrainAIService] Server stopped" << std::endl;
}

void BrainAIServiceImpl::wait() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "[BrainAIService] Waiting for server to shutdown..." << std::endl;
    
    // TODO: Implement wait
    // if (server_) {
    //     server_->Wait();
    // }
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
