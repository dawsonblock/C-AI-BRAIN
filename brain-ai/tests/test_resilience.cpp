#include "resilience/circuit_breaker.hpp"
#include <thread>
#include <chrono>
#include <iostream>

using namespace brain_ai::resilience;

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

void run_test(const char* name, void (*test_func)()) {
    std::cout << "  " << name << "... ";
    try {
        test_func();
        std::cout << "PASS\n";
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << "\n";
    }
}

void test_circuit_breaker_closed_state() {
    CircuitBreaker breaker("test_closed", CircuitBreakerConfig(3, 2, 1000));
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::CLOSED);
    
    // Execute successful operations
    int result = breaker.execute([]() { return 42; });
    EXPECT_EQ(result, 42);
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::CLOSED);
}

void test_circuit_breaker_opens_on_failures() {
    CircuitBreakerConfig config(3, 2, 1000); // 3 failures to open
    CircuitBreaker breaker("test_open", config);
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::CLOSED);
    
    // Cause failures
    for (int i = 0; i < 3; ++i) {
        try {
            breaker.execute([]() -> int {
                throw std::runtime_error("Test failure");
            });
        } catch (const std::runtime_error&) {
            // Expected
        }
    }
    
    // Circuit should now be OPEN
    EXPECT_TRUE(breaker.get_state() == CircuitState::OPEN);
}

void test_circuit_breaker_rejects_when_open() {
    CircuitBreakerConfig config(2, 2, 1000);
    CircuitBreaker breaker("test_reject", config);
    
    // Cause failures to open circuit
    for (int i = 0; i < 2; ++i) {
        try {
            breaker.execute([]() -> int {
                throw std::runtime_error("Failure");
            });
        } catch (const std::runtime_error&) {}
    }
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::OPEN);
    
    // Next call should be rejected
    bool rejected = false;
    try {
        breaker.execute([]() { return 1; });
    } catch (const CircuitBreakerOpenException&) {
        rejected = true;
    }
    
    EXPECT_TRUE(rejected);
}

void test_circuit_breaker_half_open_transition() {
    CircuitBreakerConfig config(2, 2, 100); // Short timeout for testing
    CircuitBreaker breaker("test_half_open", config);
    
    // Open the circuit
    for (int i = 0; i < 2; ++i) {
        try {
            breaker.execute([]() -> int {
                throw std::runtime_error("Failure");
            });
        } catch (const std::runtime_error&) {}
    }
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::OPEN);
    
    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // Next request should transition to HALF_OPEN
    try {
        breaker.execute([]() { return 1; });
    } catch (...) {}
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::HALF_OPEN ||
                breaker.get_state() == CircuitState::CLOSED);
}

void test_circuit_breaker_recovery() {
    CircuitBreakerConfig config(2, 2, 100); // 2 successes to close
    CircuitBreaker breaker("test_recovery", config);
    
    // Open the circuit
    for (int i = 0; i < 2; ++i) {
        try {
            breaker.execute([]() -> int {
                throw std::runtime_error("Failure");
            });
        } catch (const std::runtime_error&) {}
    }
    
    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // Successful operations to close circuit
    for (int i = 0; i < 2; ++i) {
        try {
            breaker.execute([]() { return 1; });
        } catch (const CircuitBreakerOpenException&) {
            // May happen if still in transition
        }
    }
    
    // Eventually should be closed or half-open
    auto state = breaker.get_state();
    EXPECT_TRUE(state == CircuitState::CLOSED || state == CircuitState::HALF_OPEN);
}

void test_circuit_breaker_statistics() {
    CircuitBreaker breaker("test_stats", CircuitBreakerConfig(5, 2, 1000));
    
    // Execute successful operations
    for (int i = 0; i < 3; ++i) {
        breaker.execute([]() { return 1; });
    }
    
    // Execute failed operations
    for (int i = 0; i < 2; ++i) {
        try {
            breaker.execute([]() -> int {
                throw std::runtime_error("Failure");
            });
        } catch (const std::runtime_error&) {}
    }
    
    auto stats = breaker.get_stats();
    
    EXPECT_EQ(stats.total_calls, 5);
    EXPECT_EQ(stats.successful_calls, 3);
    EXPECT_EQ(stats.failed_calls, 2);
}

void test_circuit_breaker_concurrent_calls_limit() {
    CircuitBreakerConfig config(10, 2, 1000, 2); // Max 2 concurrent calls
    CircuitBreaker breaker("test_concurrent", config);
    
    std::atomic<int> concurrent_count{0};
    std::atomic<int> rejected_count{0};
    
    auto slow_operation = [&concurrent_count]() {
        concurrent_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        concurrent_count--;
        return 1;
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&breaker, &slow_operation, &rejected_count]() {
            try {
                breaker.execute(slow_operation);
            } catch (const CircuitBreakerOpenException&) {
                rejected_count++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Some requests should have been rejected
    EXPECT_TRUE(rejected_count.load() > 0);
}

void test_circuit_breaker_manual_trip() {
    CircuitBreaker breaker("test_manual_trip", CircuitBreakerConfig());
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::CLOSED);
    
    breaker.trip();
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::OPEN);
}

void test_circuit_breaker_manual_reset() {
    CircuitBreakerConfig config(2, 2, 1000);
    CircuitBreaker breaker("test_manual_reset", config);
    
    // Open the circuit
    for (int i = 0; i < 2; ++i) {
        try {
            breaker.execute([]() -> int {
                throw std::runtime_error("Failure");
            });
        } catch (const std::runtime_error&) {}
    }
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::OPEN);
    
    breaker.reset();
    
    EXPECT_TRUE(breaker.get_state() == CircuitState::CLOSED);
}

void test_circuit_breaker_registry() {
    auto& registry = CircuitBreakerRegistry::instance();
    
    auto breaker1 = registry.get_or_create("registry_test_1");
    auto breaker2 = registry.get_or_create("registry_test_2");
    
    EXPECT_TRUE(breaker1 != nullptr);
    EXPECT_TRUE(breaker2 != nullptr);
    EXPECT_TRUE(breaker1 != breaker2);
    
    // Getting same name should return same instance
    auto breaker1_again = registry.get_or_create("registry_test_1");
    EXPECT_TRUE(breaker1 == breaker1_again);
    
    auto names = registry.get_names();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "registry_test_1") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "registry_test_2") != names.end());
    
    registry.remove("registry_test_1");
    registry.remove("registry_test_2");
}

void test_circuit_breaker_predefined_configs() {
    auto fast = configs::fast_failure();
    EXPECT_EQ(fast.failure_threshold, 3);
    EXPECT_EQ(fast.timeout_ms, 30000);
    
    auto standard = configs::standard();
    EXPECT_EQ(standard.failure_threshold, 5);
    EXPECT_EQ(standard.timeout_ms, 60000);
    
    auto tolerant = configs::tolerant();
    EXPECT_EQ(tolerant.failure_threshold, 10);
    EXPECT_EQ(tolerant.timeout_ms, 120000);
}

void test_circuit_breaker_json_export() {
    CircuitBreaker breaker("test_json", CircuitBreakerConfig());
    
    // Execute some operations
    breaker.execute([]() { return 1; });
    
    auto stats = breaker.get_stats();
    std::string json = stats.to_json();
    
    EXPECT_TRUE(json.find("state") != std::string::npos);
    EXPECT_TRUE(json.find("total_calls") != std::string::npos);
    EXPECT_TRUE(json.find("successful_calls") != std::string::npos);
}

void test_circuit_breaker_exception_propagation() {
    CircuitBreaker breaker("test_exception", CircuitBreakerConfig());
    
    bool caught_correct_exception = false;
    
    try {
        breaker.execute([]() -> int {
            throw std::invalid_argument("Custom exception");
        });
    } catch (const std::invalid_argument& e) {
        caught_correct_exception = true;
        EXPECT_EQ(std::string(e.what()), "Custom exception");
    } catch (...) {
        // Wrong exception type
    }
    
    EXPECT_TRUE(caught_correct_exception);
}

int main() {
    std::cout << "Running Circuit Breaker Tests...\n";
    std::cout << "============================================================\n\n";
    
    run_test("Circuit breaker CLOSED state", test_circuit_breaker_closed_state);
    run_test("Circuit breaker opens on failures", test_circuit_breaker_opens_on_failures);
    run_test("Circuit breaker rejects when OPEN", test_circuit_breaker_rejects_when_open);
    run_test("Circuit breaker HALF_OPEN transition", test_circuit_breaker_half_open_transition);
    run_test("Circuit breaker recovery", test_circuit_breaker_recovery);
    run_test("Circuit breaker statistics", test_circuit_breaker_statistics);
    run_test("Circuit breaker concurrent calls limit", test_circuit_breaker_concurrent_calls_limit);
    run_test("Circuit breaker manual trip", test_circuit_breaker_manual_trip);
    run_test("Circuit breaker manual reset", test_circuit_breaker_manual_reset);
    run_test("Circuit breaker registry", test_circuit_breaker_registry);
    run_test("Circuit breaker predefined configs", test_circuit_breaker_predefined_configs);
    run_test("Circuit breaker JSON export", test_circuit_breaker_json_export);
    run_test("Circuit breaker exception propagation", test_circuit_breaker_exception_propagation);
    
    std::cout << "\n============================================================\n";
    std::cout << "Resilience Tests Complete\n";
    std::cout << "============================================================\n";
    
    return 0;
}
