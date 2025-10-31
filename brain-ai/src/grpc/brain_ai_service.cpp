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

    // TODO: Implement gRPC server startup once protobuf is compiled.
    // For now, fail fast to avoid falsely reporting a running server.
    std::cerr << "[BrainAIService] gRPC server not implemented yet. Startup aborted." << std::endl;
    return false;

    // When implemented:
    // grpc::ServerBuilder builder;
    // builder.AddListeningPort(config_.server_address, grpc::InsecureServerCredentials());
    // builder.RegisterService(this);
    // server_ = builder.BuildAndStart();
    // if (!server_) return false;
    // running_.store(true);
    // if (config_.enable_reflection) { /* enable reflection */ }
    // return true;
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
