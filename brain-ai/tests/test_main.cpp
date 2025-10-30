#include <iostream>
#include <string>
#include <functional>
#include <vector>

// Simple test framework (when GTest is not available)
namespace simple_test {

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

std::vector<TestResult> results;

void EXPECT_TRUE(bool condition, const std::string& message = "") {
    if (!condition) {
        throw std::runtime_error(message.empty() ? "Expected true" : message);
    }
}

void EXPECT_FALSE(bool condition, const std::string& message = "") {
    if (condition) {
        throw std::runtime_error(message.empty() ? "Expected false" : message);
    }
}

template<typename T1, typename T2>
void EXPECT_EQ(const T1& a, const T2& b, const std::string& message = "") {
    if (!(a == b)) {
        throw std::runtime_error(message.empty() ? "Values not equal" : message);
    }
}

template<typename T1, typename T2>
void EXPECT_NE(const T1& a, const T2& b, const std::string& message = "") {
    if (a == b) {
        throw std::runtime_error(message.empty() ? "Values should not be equal" : message);
    }
}

void EXPECT_NEAR(float a, float b, float tolerance, const std::string& message = "") {
    if (std::abs(a - b) > tolerance) {
        throw std::runtime_error(message.empty() ? "Values not close enough" : message);
    }
}

void run_test(const std::string& name, std::function<void()> test_func) {
    try {
        test_func();
        results.push_back({name, true, ""});
        std::cout << "✓ " << name << "\n";
    } catch (const std::exception& e) {
        results.push_back({name, false, e.what()});
        std::cout << "✗ " << name << ": " << e.what() << "\n";
    }
}

void print_summary() {
    size_t passed = 0;
    size_t failed = 0;
    
    for (const auto& result : results) {
        if (result.passed) passed++;
        else failed++;
    }
    
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Test Summary:\n";
    std::cout << "  Total:  " << results.size() << "\n";
    std::cout << "  Passed: " << passed << " (" 
              << (100.0f * passed / results.size()) << "%)\n";
    std::cout << "  Failed: " << failed << "\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    if (failed > 0) {
        std::cout << "Failed tests:\n";
        for (const auto& result : results) {
            if (!result.passed) {
                std::cout << "  - " << result.name << ": " << result.message << "\n";
            }
        }
        std::cout << "\n";
    }
}

} // namespace simple_test

// Forward declarations of test functions
void test_utils();
void test_episodic_buffer();
void test_semantic_network();
void test_hallucination_detector();
void test_hybrid_fusion();
void test_explanation_engine();
void test_cognitive_handler();

int main() {
    std::cout << "Brain-AI v4.0 Test Suite\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    simple_test::run_test("Utils Tests", test_utils);
    simple_test::run_test("Episodic Buffer Tests", test_episodic_buffer);
    simple_test::run_test("Semantic Network Tests", test_semantic_network);
    simple_test::run_test("Hallucination Detector Tests", test_hallucination_detector);
    simple_test::run_test("Hybrid Fusion Tests", test_hybrid_fusion);
    simple_test::run_test("Explanation Engine Tests", test_explanation_engine);
    simple_test::run_test("Cognitive Handler Tests", test_cognitive_handler);
    
    simple_test::print_summary();
    
    // Return non-zero if any tests failed
    for (const auto& result : simple_test::results) {
        if (!result.passed) return 1;
    }
    
    return 0;
}
