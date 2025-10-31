#include "grpc/brain_ai_service.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

using namespace brain_ai::grpc_service;

// Signal handler for graceful shutdown
std::atomic<bool> shutdown_requested{false};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutdown signal received..." << std::endl;
        shutdown_requested.store(true);
    }
}

int main(int argc, char** argv) {
    std::cout << "=== Brain-AI gRPC Server ===" << std::endl;
    std::cout << "Version: 4.3.0" << std::endl;
    std::cout << std::endl;
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    std::string server_address = "0.0.0.0:50051";
    std::string ocr_service_url = "http://localhost:8000";
    size_t episodic_capacity = 1000;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--address" && i + 1 < argc) {
            server_address = argv[++i];
        } else if (arg == "--ocr-service" && i + 1 < argc) {
            ocr_service_url = argv[++i];
        } else if (arg == "--capacity" && i + 1 < argc) {
            episodic_capacity = std::stoul(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --address <addr>       Server address (default: 0.0.0.0:50051)" << std::endl;
            std::cout << "  --ocr-service <url>    OCR service URL (default: http://localhost:8000)" << std::endl;
            std::cout << "  --capacity <n>         Episodic buffer capacity (default: 1000)" << std::endl;
            std::cout << "  --help, -h             Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Build service
    auto service = ServiceBuilder()
        .with_address(server_address)
        .with_episodic_capacity(episodic_capacity)
        .with_ocr_service(ocr_service_url)
        .with_max_streams(100)
        .enable_reflection(true)
        .build();
    
    // Start server
    if (!service->start()) {
        std::cerr << "gRPC server is not implemented yet; exiting example gracefully." << std::endl;
        return 0;
    }

    if (!service->is_running()) {
        std::cerr << "gRPC server is not running (not implemented); exiting." << std::endl;
        return 0;
    }

    std::cout << std::endl;
    std::cout << "Server Configuration:" << std::endl;
    std::cout << "  Address: " << server_address << std::endl;
    std::cout << "  OCR Service: " << ocr_service_url << std::endl;
    std::cout << "  Episodic Capacity: " << episodic_capacity << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    std::cout << std::endl;

    // Wait for shutdown signal
    while (!shutdown_requested.load() && service->is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        static int counter = 0;
        if (++counter % 60 == 0) {
            auto stats = service->get_stats();
            std::cout << "Stats: "
                      << "Queries=" << stats.total_queries.load()
                      << " (" << stats.successful_queries.load() << " ok, "
                      << stats.failed_queries.load() << " failed), "
                      << "Docs=" << stats.total_documents.load()
                      << " (" << stats.successful_documents.load() << " ok, "
                      << stats.failed_documents.load() << " failed), "
                      << "Uptime=" << stats.uptime_seconds() << "s"
                      << std::endl;
        }
    }
    
    // Graceful shutdown
    service->stop();
    
    // Print final stats
    auto stats = service->get_stats();
    std::cout << std::endl;
    std::cout << "Final Statistics:" << std::endl;
    std::cout << "  Total Queries: " << stats.total_queries.load() << std::endl;
    std::cout << "    Successful: " << stats.successful_queries.load() << std::endl;
    std::cout << "    Failed: " << stats.failed_queries.load() << std::endl;
    std::cout << "  Total Documents: " << stats.total_documents.load() << std::endl;
    std::cout << "    Successful: " << stats.successful_documents.load() << std::endl;
    std::cout << "    Failed: " << stats.failed_documents.load() << std::endl;
    std::cout << "  Uptime: " << stats.uptime_seconds() << " seconds" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Server shutdown complete." << std::endl;
    
    return 0;
}
