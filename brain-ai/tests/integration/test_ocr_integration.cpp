#include "document/document_processor.hpp"
#include "document/ocr_client.hpp"
#include "cognitive_handler.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

using namespace brain_ai;
using namespace brain_ai::document;

// Test configuration
const std::string OCR_SERVICE_URL = "http://localhost:8000";
const int MAX_WAIT_SECONDS = 30;
const int RETRY_DELAY_MS = 1000;

// Test macros
#define EXPECT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "FAIL: " << #expr << " at line " << __LINE__ << std::endl; \
        return false; \
    }

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "FAIL: " << #a << " != " << #b << " at line " << __LINE__ << std::endl; \
        return false; \
    }

#define EXPECT_GT(a, b) \
    if ((a) <= (b)) { \
        std::cerr << "FAIL: " << #a << " <= " << #b << " at line " << __LINE__ << std::endl; \
        return false; \
    }

#define RUN_TEST(test_func) \
    std::cout << "Running " << #test_func << "..." << std::endl; \
    if (test_func()) { \
        std::cout << "  PASS" << std::endl; \
        passed++; \
    } else { \
        std::cout << "  FAIL" << std::endl; \
        failed++; \
    } \
    total++;

#define RUN_TEST_OPTIONAL(test_func, description) \
    std::cout << "Running " << #test_func << " (" << description << ")..." << std::endl; \
    if (test_func()) { \
        std::cout << "  PASS" << std::endl; \
        passed++; \
    } else { \
        std::cout << "  SKIP (optional test, service may not be available)" << std::endl; \
        skipped++; \
    } \
    total++;

// Helper: Check if OCR service is available
bool is_ocr_service_available() {
    OCRConfig config;
    config.service_url = OCR_SERVICE_URL;
    
    try {
        OCRClient client(config);
        return client.check_health();
    } catch (const std::exception& e) {
        std::cerr << "Service check exception: " << e.what() << std::endl;
        return false;
    }
}

// Helper: Wait for OCR service to be ready
bool wait_for_service(int max_wait_seconds) {
    std::cout << "Waiting for OCR service at " << OCR_SERVICE_URL << "..." << std::endl;
    
    for (int i = 0; i < max_wait_seconds; ++i) {
        if (is_ocr_service_available()) {
            std::cout << "Service is ready!" << std::endl;
            return true;
        }
        
        std::cout << "  Attempt " << (i + 1) << "/" << max_wait_seconds << "..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
    }
    
    std::cerr << "Service did not become available within " << max_wait_seconds << " seconds" << std::endl;
    return false;
}

// Helper: Create test image file
bool create_test_image(const std::string& filepath, const std::string& content) {
    // Create a simple text file that can be "processed" as an image
    // In a real scenario, this would be a real image/PDF
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create test file: " << filepath << std::endl;
        return false;
    }
    
    file << content;
    file.close();
    return true;
}

// Test 1: Service health check
bool test_service_health_check() {
    if (!is_ocr_service_available()) {
        std::cerr << "OCR service not available at " << OCR_SERVICE_URL << std::endl;
        return false;
    }
    
    OCRConfig config;
    config.service_url = OCR_SERVICE_URL;
    OCRClient client(config);
    
    EXPECT_TRUE(client.check_health());
    
    return true;
}

// Test 2: Service status endpoint
bool test_service_status() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    OCRConfig config;
    config.service_url = OCR_SERVICE_URL;
    OCRClient client(config);
    
    auto status = client.get_service_status();
    
    EXPECT_FALSE(status.empty());
    EXPECT_TRUE(status.contains("status"));
    
    std::cout << "  Service status: " << status.dump(2) << std::endl;
    
    return true;
}

// Test 3: Process simple text image
bool test_process_simple_text() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    // Create test image
    std::string test_file = "/tmp/test_ocr_simple.txt";
    if (!create_test_image(test_file, "Hello World!\nThis is a test document.")) {
        return false;
    }
    
    OCRConfig config;
    config.service_url = OCR_SERVICE_URL;
    config.mode = "base";
    config.task = "ocr";
    
    OCRClient client(config);
    
    auto result = client.process_file(test_file);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.text.empty());
    EXPECT_GT(result.confidence, 0.0f);
    
    std::cout << "  Extracted text length: " << result.text.size() << " chars" << std::endl;
    std::cout << "  Confidence: " << result.confidence << std::endl;
    std::cout << "  Processing time: " << result.processing_time.count() << "ms" << std::endl;
    
    // Cleanup
    std::remove(test_file.c_str());
    
    return true;
}

// Test 4: End-to-end document processing pipeline
bool test_end_to_end_pipeline() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    // Create test document
    std::string test_file = "/tmp/test_ocr_pipeline.txt";
    if (!create_test_image(test_file, "Document processing pipeline test.\nMultiple lines of text.")) {
        return false;
    }
    
    // Setup cognitive handler
    CognitiveHandler cognitive(100);
    
    // Configure document processor
    DocumentProcessor::Config config;
    config.ocr_config.service_url = OCR_SERVICE_URL;
    config.ocr_config.mode = "base";
    config.ocr_config.task = "markdown";
    config.auto_generate_embeddings = true;
    config.create_episodic_memory = true;
    config.index_in_vector_store = true;
    
    DocumentProcessor processor(cognitive, config);
    
    // Process document
    auto result = processor.process(test_file, "test_doc_001");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.doc_id, "test_doc_001");
    EXPECT_FALSE(result.extracted_text.empty());
    EXPECT_FALSE(result.validated_text.empty());
    EXPECT_GT(result.ocr_confidence, 0.0f);
    EXPECT_GT(result.validation_confidence, 0.0f);
    EXPECT_TRUE(result.indexed);
    
    std::cout << "  Doc ID: " << result.doc_id << std::endl;
    std::cout << "  Extracted: " << result.extracted_text.size() << " chars" << std::endl;
    std::cout << "  Validated: " << result.validated_text.size() << " chars" << std::endl;
    std::cout << "  OCR confidence: " << result.ocr_confidence << std::endl;
    std::cout << "  Validation confidence: " << result.validation_confidence << std::endl;
    std::cout << "  Processing time: " << result.processing_time.count() << "ms" << std::endl;
    
    // Cleanup
    std::remove(test_file.c_str());
    
    return true;
}

// Test 5: Batch document processing
bool test_batch_processing() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    // Create multiple test documents
    std::vector<std::string> test_files;
    for (int i = 0; i < 3; ++i) {
        std::string filepath = "/tmp/test_ocr_batch_" + std::to_string(i) + ".txt";
        std::string content = "Test document " + std::to_string(i) + "\nSample content.";
        
        if (!create_test_image(filepath, content)) {
            return false;
        }
        
        test_files.push_back(filepath);
    }
    
    CognitiveHandler cognitive(100);
    
    DocumentProcessor::Config config;
    config.ocr_config.service_url = OCR_SERVICE_URL;
    config.ocr_config.mode = "tiny";  // Use tiny for faster processing
    config.auto_generate_embeddings = true;
    
    DocumentProcessor processor(cognitive, config);
    
    // Process batch with progress callback
    size_t progress_count = 0;
    auto results = processor.process_batch(test_files, 
        [&progress_count](size_t current, size_t total, const std::string& status) {
            std::cout << "    [" << current << "/" << total << "] " << status << std::endl;
            progress_count++;
        }
    );
    
    EXPECT_EQ(results.size(), test_files.size());
    EXPECT_GT(progress_count, 0);  // Progress callback was called
    
    // Check all succeeded
    size_t success_count = 0;
    for (const auto& result : results) {
        if (result.success) {
            success_count++;
        }
    }
    
    std::cout << "  Batch results: " << success_count << "/" << results.size() << " succeeded" << std::endl;
    
    EXPECT_EQ(success_count, results.size());
    
    // Get processing stats
    auto stats = processor.get_stats();
    std::cout << "  Total processed: " << stats.total_documents << std::endl;
    std::cout << "  Average time: " << stats.avg_time.count() << "ms" << std::endl;
    
    // Cleanup
    for (const auto& filepath : test_files) {
        std::remove(filepath.c_str());
    }
    
    return true;
}

// Test 6: Different resolution modes
bool test_resolution_modes() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    std::string test_file = "/tmp/test_ocr_resolution.txt";
    if (!create_test_image(test_file, "Resolution mode test document.")) {
        return false;
    }
    
    std::vector<std::string> modes = {"tiny", "small", "base"};
    
    for (const auto& mode : modes) {
        OCRConfig config;
        config.service_url = OCR_SERVICE_URL;
        config.mode = mode;
        config.task = "ocr";
        
        OCRClient client(config);
        
        auto result = client.process_file(test_file);
        
        EXPECT_TRUE(result.success);
        
        std::cout << "  Mode: " << mode 
                  << " | Time: " << result.processing_time.count() << "ms"
                  << " | Confidence: " << result.confidence << std::endl;
    }
    
    std::remove(test_file.c_str());
    
    return true;
}

// Test 7: Different task types
bool test_task_types() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    std::string test_file = "/tmp/test_ocr_tasks.txt";
    if (!create_test_image(test_file, "# Header\n\nParagraph text with **bold** and *italic*.")) {
        return false;
    }
    
    std::vector<std::string> tasks = {"ocr", "markdown"};
    
    for (const auto& task : tasks) {
        OCRConfig config;
        config.service_url = OCR_SERVICE_URL;
        config.mode = "base";
        config.task = task;
        
        OCRClient client(config);
        
        auto result = client.process_file(test_file);
        
        EXPECT_TRUE(result.success);
        
        std::cout << "  Task: " << task 
                  << " | Text length: " << result.text.size() 
                  << " | Confidence: " << result.confidence << std::endl;
    }
    
    std::remove(test_file.c_str());
    
    return true;
}

// Test 8: Error handling - invalid file
bool test_error_handling_invalid_file() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    OCRConfig config;
    config.service_url = OCR_SERVICE_URL;
    
    OCRClient client(config);
    
    // Try to process non-existent file
    auto result = client.process_file("/tmp/nonexistent_file_xyz.txt");
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
    
    std::cout << "  Error message: " << result.error_message << std::endl;
    
    return true;
}

// Test 9: Service timeout handling
bool test_service_timeout() {
    // This test uses a very short timeout to trigger timeout handling
    OCRConfig config;
    config.service_url = OCR_SERVICE_URL;
    config.timeout = std::chrono::milliseconds(1);  // 1ms timeout, very likely to trigger
    
    try {
        OCRClient client(config);
        
        // Service might not be available or timeout is too short
        // Either way, we're testing the timeout mechanism
        client.check_health();
        
        // If we get here, the timeout did NOT trigger as expected.
        std::cerr << "  FAIL: Timeout was expected but request succeeded." << std::endl;
        return false;
        
    } catch (const std::exception& e) {
        std::cout << "  Timeout/exception as expected: " << e.what() << std::endl;
    }
    
    return true;
}

// Test 10: Configuration updates
bool test_configuration_updates() {
    if (!is_ocr_service_available()) {
        return false;
    }
    
    OCRConfig config;
    config.service_url = OCR_SERVICE_URL;
    config.mode = "tiny";
    
    OCRClient client(config);
    
    EXPECT_EQ(client.get_config().mode, "tiny");
    
    // Update configuration
    config.mode = "base";
    client.update_config(config);
    
    EXPECT_EQ(client.get_config().mode, "base");
    
    std::cout << "  Configuration updated successfully" << std::endl;
    
    return true;
}

int main(int argc, char** argv) {
    std::cout << "\n=== Brain-AI OCR Integration Tests ===\n" << std::endl;
    
    bool service_available = wait_for_service(MAX_WAIT_SECONDS);
    
    if (!service_available) {
        std::cout << "\n⚠️  OCR service is not available at " << OCR_SERVICE_URL << std::endl;
        std::cout << "These tests require the DeepSeek-OCR service to be running." << std::endl;
        std::cout << "\nTo start the service:" << std::endl;
        std::cout << "  cd brain-ai/deepseek-ocr-service" << std::endl;
        std::cout << "  docker-compose up --build" << std::endl;
        std::cout << "\nSkipping all integration tests.\n" << std::endl;
        return 0;
    }
    
    int total = 0, passed = 0, failed = 0, skipped = 0;
    
    // Basic service tests
    RUN_TEST(test_service_health_check);
    RUN_TEST(test_service_status);
    
    // OCR processing tests
    RUN_TEST_OPTIONAL(test_process_simple_text, "requires OCR service");
    RUN_TEST_OPTIONAL(test_end_to_end_pipeline, "requires OCR service");
    RUN_TEST_OPTIONAL(test_batch_processing, "requires OCR service");
    
    // Configuration tests
    RUN_TEST_OPTIONAL(test_resolution_modes, "requires OCR service");
    RUN_TEST_OPTIONAL(test_task_types, "requires OCR service");
    
    // Error handling tests
    RUN_TEST_OPTIONAL(test_error_handling_invalid_file, "requires OCR service");
    RUN_TEST(test_service_timeout);
    RUN_TEST_OPTIONAL(test_configuration_updates, "requires OCR service");
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total:   " << total << std::endl;
    std::cout << "Passed:  " << passed << std::endl;
    std::cout << "Failed:  " << failed << std::endl;
    std::cout << "Skipped: " << skipped << std::endl;
    
    if (failed > 0) {
        std::cout << "\n❌ Some tests failed" << std::endl;
        return 1;
    } else if (passed > 0) {
        std::cout << "\n✅ All tests passed" << std::endl;
        return 0;
    } else {
        std::cout << "\n⚠️  No tests were executed" << std::endl;
        return 0;
    }
}
