#include "vector_search/hnsw_index.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <random>
#include <cmath>

using namespace brain_ai::vector_search;

// Simple test framework macros
#define EXPECT_EQ(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            std::cerr << "FAIL: " << #actual << " == " << #expected \
                      << " (actual: " << (actual) << ", expected: " << (expected) << ")\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << #condition << " is not true\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_FALSE(condition) \
    do { \
        if ((condition)) { \
            std::cerr << "FAIL: " << #condition << " is not false\n"; \
            return; \
        } \
    } while(0)

#define EXPECT_NEAR(actual, expected, tolerance) \
    do { \
        double _actual = (actual); \
        double _expected = (expected); \
        double _tolerance = (tolerance); \
        if (std::abs(_actual - _expected) > _tolerance) { \
            std::cerr << "FAIL: " << #actual << " near " << #expected \
                      << " (actual: " << _actual << ", expected: " << _expected \
                      << ", tolerance: " << _tolerance << ")\n"; \
            return; \
        } \
    } while(0)

void run_test(const char* name, void (*test_func)()) {
    std::cout << "  " << name << "... ";
    try {
        test_func();
        std::cout << "PASS\n";
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << "\n";
    }
}

// Helper: Generate random embedding
std::vector<float> random_embedding(size_t dim, std::mt19937& gen) {
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> emb(dim);
    for (auto& val : emb) {
        val = dist(gen);
    }
    return emb;
}

// Helper: Normalize vector
void normalize(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float val : vec) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-10f) {
        for (float& val : vec) {
            val /= norm;
        }
    }
}

// Helper: Cosine similarity
float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    float dot = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
    }
    return dot;
}

// ============================================================================
// Tests
// ============================================================================

void test_index_creation() {
    HNSWIndex index(128);
    EXPECT_EQ(index.size(), 0);
    
    auto stats = index.get_statistics();
    EXPECT_EQ(stats.dimension, 128);
    EXPECT_EQ(stats.total_documents, 0);
}

void test_add_document() {
    HNSWIndex index(64);
    
    std::vector<float> emb(64, 0.1f);
    bool added = index.add_document("doc1", emb, "Test document");
    
    EXPECT_TRUE(added);
    EXPECT_EQ(index.size(), 1);
    EXPECT_TRUE(index.has_document("doc1"));
}

void test_add_duplicate_document() {
    HNSWIndex index(64);
    
    std::vector<float> emb(64, 0.1f);
    index.add_document("doc1", emb, "Test document");
    
    bool added_again = index.add_document("doc1", emb, "Duplicate");
    EXPECT_FALSE(added_again);
    EXPECT_EQ(index.size(), 1);
}

void test_search_single_document() {
    HNSWIndex index(64);
    
    std::vector<float> emb(64, 0.1f);
    normalize(emb);
    index.add_document("doc1", emb, "Test document");
    
    // Search with same embedding
    auto results = index.search(emb, 1);
    
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].doc_id, "doc1");
    EXPECT_EQ(results[0].content, "Test document");
    EXPECT_NEAR(results[0].similarity, 1.0f, 0.05f);  // Should be very similar to itself
}

void test_search_multiple_documents() {
    HNSWIndex index(64);
    std::mt19937 gen(42);
    
    // Add 10 random documents
    for (int i = 0; i < 10; ++i) {
        auto emb = random_embedding(64, gen);
        normalize(emb);
        index.add_document("doc" + std::to_string(i), emb, "Document " + std::to_string(i));
    }
    
    EXPECT_EQ(index.size(), 10);
    
    // Search
    auto query = random_embedding(64, gen);
    normalize(query);
    auto results = index.search(query, 5);
    
    EXPECT_EQ(results.size(), 5);
    
    // Results should be sorted by similarity (highest first)
    for (size_t i = 0; i < results.size() - 1; ++i) {
        EXPECT_TRUE(results[i].similarity >= results[i+1].similarity);
    }
}

void test_search_relevance() {
    HNSWIndex index(64);
    
    // Create a query embedding
    std::vector<float> query(64, 0.0f);
    query[0] = 1.0f;
    normalize(query);
    
    // Create similar document (aligned with query)
    std::vector<float> similar = query;
    similar[1] = 0.1f;
    normalize(similar);
    
    // Create dissimilar document (orthogonal to query)
    std::vector<float> dissimilar(64, 0.0f);
    dissimilar[10] = 1.0f;
    normalize(dissimilar);
    
    index.add_document("similar", similar, "Similar document");
    index.add_document("dissimilar", dissimilar, "Dissimilar document");
    
    auto results = index.search(query, 2);
    
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].doc_id, "similar");  // Most similar should be first
    EXPECT_TRUE(results[0].similarity > results[1].similarity);
}

void test_remove_document() {
    HNSWIndex index(64);
    
    std::vector<float> emb(64, 0.1f);
    index.add_document("doc1", emb, "Document 1");
    index.add_document("doc2", emb, "Document 2");
    
    EXPECT_EQ(index.size(), 2);
    
    bool removed = index.remove_document("doc1");
    EXPECT_TRUE(removed);
    EXPECT_EQ(index.size(), 1);
    EXPECT_FALSE(index.has_document("doc1"));
    EXPECT_TRUE(index.has_document("doc2"));
}

void test_get_document() {
    HNSWIndex index(64);
    
    std::vector<float> emb(64, 0.1f);
    nlohmann::json metadata = {{"key", "value"}};
    index.add_document("doc1", emb, "Test content", metadata);
    
    auto doc = index.get_document("doc1");
    EXPECT_EQ(doc.doc_id, "doc1");
    EXPECT_EQ(doc.content, "Test content");
    EXPECT_EQ(doc.metadata["key"], "value");
}

void test_clear_index() {
    HNSWIndex index(64);
    
    std::vector<float> emb(64, 0.1f);
    index.add_document("doc1", emb, "Document 1");
    index.add_document("doc2", emb, "Document 2");
    
    EXPECT_EQ(index.size(), 2);
    
    index.clear();
    EXPECT_EQ(index.size(), 0);
    EXPECT_FALSE(index.has_document("doc1"));
    EXPECT_FALSE(index.has_document("doc2"));
}

void test_ef_search_parameter() {
    HNSWIndex index(64);
    
    EXPECT_EQ(index.get_ef_search(), 50);  // Default value
    
    index.set_ef_search(100);
    EXPECT_EQ(index.get_ef_search(), 100);
}

void test_statistics() {
    HNSWIndex index(128, 1000, 16, 200);
    std::mt19937 gen(42);
    
    // Add some documents
    for (int i = 0; i < 5; ++i) {
        auto emb = random_embedding(128, gen);
        index.add_document("doc" + std::to_string(i), emb, "Document " + std::to_string(i));
    }
    
    auto stats = index.get_statistics();
    EXPECT_EQ(stats.total_documents, 5);
    EXPECT_EQ(stats.dimension, 128);
    EXPECT_EQ(stats.max_elements, 1000);
    EXPECT_EQ(stats.M, 16);
    EXPECT_EQ(stats.ef_construction, 200);
    EXPECT_TRUE(stats.memory_usage_mb > 0.0);
}

void test_save_and_load() {
    const std::string filepath = "/tmp/test_hnsw_index.bin";
    std::mt19937 gen(42);
    
    // Create and populate index
    {
        HNSWIndex index(64);
        for (int i = 0; i < 5; ++i) {
            auto emb = random_embedding(64, gen);
            index.add_document("doc" + std::to_string(i), emb, "Document " + std::to_string(i));
        }
        
        bool saved = index.save(filepath);
        EXPECT_TRUE(saved);
    }
    
    // Load index
    {
        HNSWIndex index(64);  // Dimension must match
        bool loaded = index.load(filepath);
        EXPECT_TRUE(loaded);
        
        EXPECT_EQ(index.size(), 5);
        EXPECT_TRUE(index.has_document("doc0"));
        EXPECT_TRUE(index.has_document("doc4"));
        
        auto doc = index.get_document("doc0");
        EXPECT_EQ(doc.content, "Document 0");
    }
    
    // Clean up
    std::remove(filepath.c_str());
    std::remove((filepath + ".meta").c_str());
}

void test_large_index() {
    HNSWIndex index(128);
    std::mt19937 gen(42);
    
    // Add 1000 documents
    for (int i = 0; i < 1000; ++i) {
        auto emb = random_embedding(128, gen);
        index.add_document("doc" + std::to_string(i), emb, "Document " + std::to_string(i));
    }
    
    EXPECT_EQ(index.size(), 1000);
    
    // Search should still work
    auto query = random_embedding(128, gen);
    auto results = index.search(query, 10);
    EXPECT_EQ(results.size(), 10);
}

void test_index_builder() {
    auto index = IndexBuilder()
        .dimension(256)
        .max_elements(5000)
        .M(32)
        .ef_construction(400)
        .space_type("ip")
        .build();
    
    auto stats = index->get_statistics();
    EXPECT_EQ(stats.dimension, 256);
    EXPECT_EQ(stats.max_elements, 5000);
    EXPECT_EQ(stats.M, 32);
    EXPECT_EQ(stats.ef_construction, 400);
}

void test_thread_safety() {
    HNSWIndex index(64);
    std::mt19937 gen(42);
    
    // Pre-populate with some documents
    for (int i = 0; i < 50; ++i) {
        auto emb = random_embedding(64, gen);
        index.add_document("doc" + std::to_string(i), emb, "Document " + std::to_string(i));
    }
    
    // Concurrent searches
    std::vector<std::thread> threads;
    std::atomic<int> search_count{0};
    
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([&index, &gen, &search_count, t]() {
            std::mt19937 local_gen(42 + t);  // Use thread index for seed
            for (int i = 0; i < 20; ++i) {
                auto query = random_embedding(64, local_gen);
                auto results = index.search(query, 5);
                if (results.size() > 0) {
                    search_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(search_count.load(), 100);  // 5 threads * 20 searches
}

void test_dimension_validation() {
    HNSWIndex index(64);
    
    // Try to add document with wrong dimension
    std::vector<float> wrong_dim(32, 0.1f);
    
    bool exception_thrown = false;
    try {
        index.add_document("doc1", wrong_dim, "Wrong dimension");
    } catch (const std::invalid_argument& e) {
        exception_thrown = true;
    }
    
    EXPECT_TRUE(exception_thrown);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "Running Vector Search Tests...\n";
    std::cout << "============================================================\n\n";
    
    // Basic operations
    run_test("Index creation", test_index_creation);
    run_test("Add document", test_add_document);
    run_test("Add duplicate document", test_add_duplicate_document);
    
    // Search functionality
    run_test("Search single document", test_search_single_document);
    run_test("Search multiple documents", test_search_multiple_documents);
    run_test("Search relevance ranking", test_search_relevance);
    
    // Document management
    run_test("Remove document", test_remove_document);
    run_test("Get document", test_get_document);
    run_test("Clear index", test_clear_index);
    
    // Configuration
    run_test("EF search parameter", test_ef_search_parameter);
    run_test("Index statistics", test_statistics);
    
    // Persistence
    run_test("Save and load index", test_save_and_load);
    
    // Performance and scale
    run_test("Large index (1000 docs)", test_large_index);
    
    // Builders and utilities
    run_test("Index builder pattern", test_index_builder);
    
    // Thread safety
    run_test("Thread-safe concurrent searches", test_thread_safety);
    
    // Error handling
    run_test("Dimension validation", test_dimension_validation);
    
    std::cout << "\n============================================================\n";
    std::cout << "Vector Search Tests Complete\n";
    std::cout << "============================================================\n";
    
    return 0;
}
