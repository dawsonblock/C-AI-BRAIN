#include "document/document_processor.hpp"
#include "document/ocr_client.hpp"
#include "document/text_validator.hpp"
#include "cognitive_handler.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace brain_ai;
using namespace brain_ai::document;

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

// Test OCRClient configuration
bool test_ocr_client_config() {
    OCRConfig config;
    config.service_url = "http://localhost:8000";
    config.mode = "base";
    config.task = "markdown";
    config.max_tokens = 4096;
    
    EXPECT_EQ(config.service_url, "http://localhost:8000");
    EXPECT_EQ(config.mode, "base");
    EXPECT_EQ(config.task, "markdown");
    EXPECT_EQ(config.max_tokens, 4096);
    
    return true;
}

// Test TextValidator with sample text
bool test_text_validator_basic() {
    ValidationConfig config;
    config.remove_ocr_artifacts = true;
    config.fix_spacing = true;
    
    TextValidator validator(config);
    
    // Test with spaces
    std::string text_with_spaces = "This  is   some    text.";
    auto result = validator.validate(text_with_spaces);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.cleaned_text.find("  ") == std::string::npos); // No double spaces
    
    return true;
}

// Test TextValidator with OCR artifacts
bool test_text_validator_artifacts() {
    ValidationConfig config;
    config.remove_ocr_artifacts = true;
    
    TextValidator validator(config);
    
    std::string text_with_artifacts = "Hello � World";
    auto result = validator.validate(text_with_artifacts);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.cleaned_text.find("�") == std::string::npos);
    
    return true;
}

// Test TextValidator with line breaks
bool test_text_validator_line_breaks() {
    ValidationConfig config;
    config.fix_line_breaks = true;
    
    TextValidator validator(config);
    
    // Hyphenated line break
    std::string text = "hyp-\nhenated";
    auto result = validator.validate(text);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.cleaned_text == "hyphenated");
    
    return true;
}

// Test TextValidator confidence calculation
bool test_text_validator_confidence() {
    ValidationConfig config;
    
    TextValidator validator(config);
    
    // Clean text should have high confidence
    std::string clean_text = "This is perfectly clean text.";
    auto result = validator.validate(clean_text);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.confidence > 0.8f);
    EXPECT_EQ(result.errors_corrected, 0);
    
    return true;
}

// Test TextValidator with empty text
bool test_text_validator_empty() {
    TextValidator validator;
    
    auto result = validator.validate("");
    
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.confidence, 0.0f);
    EXPECT_TRUE(result.warnings.size() > 0);
    
    return true;
}

// Test DocumentProcessor configuration
bool test_document_processor_config() {
    CognitiveHandler cognitive(100);
    
    DocumentProcessor::Config config;
    config.ocr_config.service_url = "http://localhost:8000";
    config.auto_generate_embeddings = true;
    config.create_episodic_memory = true;
    config.index_in_vector_store = true;
    
    DocumentProcessor processor(cognitive, config);
    
    const auto& proc_config = processor.get_config();
    EXPECT_EQ(proc_config.ocr_config.service_url, "http://localhost:8000");
    EXPECT_TRUE(proc_config.auto_generate_embeddings);
    EXPECT_TRUE(proc_config.create_episodic_memory);
    EXPECT_TRUE(proc_config.index_in_vector_store);
    
    return true;
}

// Test DocumentProcessor statistics
bool test_document_processor_stats() {
    CognitiveHandler cognitive(100);
    DocumentProcessor::Config config;
    DocumentProcessor processor(cognitive, config);
    
    auto stats = processor.get_stats();
    EXPECT_EQ(stats.total_documents, 0);
    EXPECT_EQ(stats.successful, 0);
    EXPECT_EQ(stats.failed, 0);
    
    processor.reset_stats();
    stats = processor.get_stats();
    EXPECT_EQ(stats.total_documents, 0);
    
    return true;
}

// Test OCRResult structure
bool test_ocr_result_structure() {
    OCRResult result;
    
    result.text = "Extracted text";
    result.confidence = 0.95f;
    result.success = true;
    result.processing_time = std::chrono::milliseconds(100);
    
    EXPECT_EQ(result.text, "Extracted text");
    EXPECT_EQ(result.confidence, 0.95f);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.processing_time.count(), 100);
    
    return true;
}

// Test DocumentResult structure
bool test_document_result_structure() {
    DocumentResult result;
    
    result.doc_id = "test_doc_123";
    result.extracted_text = "Raw OCR text";
    result.validated_text = "Cleaned text";
    result.ocr_confidence = 0.9f;
    result.validation_confidence = 0.85f;
    result.indexed = true;
    result.success = true;
    
    EXPECT_EQ(result.doc_id, "test_doc_123");
    EXPECT_EQ(result.extracted_text, "Raw OCR text");
    EXPECT_EQ(result.validated_text, "Cleaned text");
    EXPECT_TRUE(result.indexed);
    EXPECT_TRUE(result.success);
    
    return true;
}

// Test ValidationResult structure
bool test_validation_result_structure() {
    ValidationResult result;
    
    result.cleaned_text = "Cleaned text";
    result.confidence = 0.88f;
    result.errors_corrected = 3;
    result.is_valid = true;
    result.warnings.push_back("Minor issue");
    
    EXPECT_EQ(result.cleaned_text, "Cleaned text");
    EXPECT_EQ(result.confidence, 0.88f);
    EXPECT_EQ(result.errors_corrected, 3);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.warnings.size(), 1);
    
    return true;
}

// Test TextValidator Unicode normalization
bool test_text_validator_unicode() {
    ValidationConfig config;
    config.normalize_unicode = true;
    
    TextValidator validator(config);
    
    // Using hex escapes for Unicode characters
    std::string text_with_unicode = "Hello \xE2\x80\x9Csmart quotes\xE2\x80\x9D and \xE2\x80\x98apostrophes\xE2\x80\x99";
    auto result = validator.validate(text_with_unicode);
    
    EXPECT_TRUE(result.is_valid);
    // Check that smart quotes were normalized to regular quotes
    EXPECT_TRUE(result.cleaned_text.find("\"") != std::string::npos);
    
    return true;
}

// Test TextValidator control character removal
bool test_text_validator_control_chars() {
    ValidationConfig config;
    config.remove_control_chars = true;
    
    TextValidator validator(config);
    
    std::string text_with_control = "Hello\x01\x02World";
    auto result = validator.validate(text_with_control);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.cleaned_text, "HelloWorld");
    
    return true;
}

// Test ProcessingStats update
bool test_processing_stats_update() {
    ProcessingStats stats;
    
    stats.total_documents = 10;
    stats.successful = 8;
    stats.failed = 2;
    stats.total_time = std::chrono::milliseconds(5000);
    stats.update();
    
    EXPECT_EQ(stats.total_documents, 10);
    EXPECT_EQ(stats.successful, 8);
    EXPECT_EQ(stats.failed, 2);
    EXPECT_EQ(stats.avg_time.count(), 500); // 5000ms / 10 docs = 500ms avg
    
    return true;
}

int main() {
    std::cout << "\n=== Brain-AI Document Processing Tests ===\n" << std::endl;
    
    int total = 0, passed = 0, failed = 0;
    
    // OCR Client tests
    RUN_TEST(test_ocr_client_config);
    RUN_TEST(test_ocr_result_structure);
    
    // Text Validator tests
    RUN_TEST(test_text_validator_basic);
    RUN_TEST(test_text_validator_artifacts);
    RUN_TEST(test_text_validator_line_breaks);
    RUN_TEST(test_text_validator_confidence);
    RUN_TEST(test_text_validator_empty);
    RUN_TEST(test_text_validator_unicode);
    RUN_TEST(test_text_validator_control_chars);
    RUN_TEST(test_validation_result_structure);
    
    // Document Processor tests
    RUN_TEST(test_document_processor_config);
    RUN_TEST(test_document_processor_stats);
    RUN_TEST(test_document_result_structure);
    RUN_TEST(test_processing_stats_update);
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total:  " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    
    return (failed == 0) ? 0 : 1;
}
