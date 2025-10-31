#ifndef BRAIN_AI_RESILIENCE_CIRCUIT_BREAKER_HPP
#define BRAIN_AI_RESILIENCE_CIRCUIT_BREAKER_HPP

#include <string>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>

namespace brain_ai {
namespace resilience {

// Circuit breaker states
enum class CircuitState {
    CLOSED,      // Normal operation, requests pass through
    OPEN,        // Circuit is open, requests fail fast
    HALF_OPEN    // Testing if service has recovered
};

// Convert circuit state to string
inline const char* circuit_state_to_string(CircuitState state) {
    switch (state) {
        case CircuitState::CLOSED: return "CLOSED";
        case CircuitState::OPEN: return "OPEN";
        case CircuitState::HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

// Circuit breaker configuration
struct CircuitBreakerConfig {
    size_t failure_threshold = 5;           // Failures before opening
    size_t success_threshold = 2;           // Successes to close from half-open
    int timeout_ms = 60000;                 // Time to wait before half-open (60s)
    int max_concurrent_calls = 100;         // Max concurrent calls allowed
    
    CircuitBreakerConfig() = default;
    
    CircuitBreakerConfig(size_t failure_thresh,
                        size_t success_thresh,
                        int timeout,
                        int max_concurrent = 100)
        : failure_threshold(failure_thresh)
        , success_threshold(success_thresh)
        , timeout_ms(timeout)
        , max_concurrent_calls(max_concurrent) {}
};

// Circuit breaker statistics
struct CircuitBreakerStats {
    CircuitState state;
    size_t total_calls = 0;
    size_t successful_calls = 0;
    size_t failed_calls = 0;
    size_t rejected_calls = 0;
    size_t consecutive_failures = 0;
    size_t consecutive_successes = 0;
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point last_state_change_time;
    int current_concurrent_calls = 0;
    
    // Format as JSON string
    std::string to_json() const;
};

// Circuit breaker exception
class CircuitBreakerOpenException : public std::runtime_error {
public:
    explicit CircuitBreakerOpenException(const std::string& name)
        : std::runtime_error("Circuit breaker '" + name + "' is OPEN") {}
};

// Circuit breaker implementation
class CircuitBreaker {
public:
    explicit CircuitBreaker(const std::string& name,
                           const CircuitBreakerConfig& config = CircuitBreakerConfig())
        : name_(name)
        , config_(config)
        , state_(CircuitState::CLOSED)
        , stats_()
        , concurrent_calls_(0) {
        stats_.state = state_.load();
        stats_.last_state_change_time = std::chrono::system_clock::now();
    }
    
    // Execute a function with circuit breaker protection
    template<typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) -> decltype(func(args...)) {
        // Check if circuit allows call
        if (!allow_request()) {
            stats_.rejected_calls++;
            throw CircuitBreakerOpenException(name_);
        }
        
        // Track concurrent calls
        concurrent_calls_++;
        stats_.current_concurrent_calls = concurrent_calls_.load();
        
        try {
            // Execute the function
            auto result = func(std::forward<Args>(args)...);
            
            // Record success
            on_success();
            
            concurrent_calls_--;
            return result;
            
        } catch (...) {
            // Record failure
            on_failure();
            
            concurrent_calls_--;
            throw;
        }
    }
    
    // Manually trip the circuit breaker
    void trip();
    
    // Manually reset the circuit breaker
    void reset();
    
    // Get current state
    CircuitState get_state() const {
        return state_.load(std::memory_order_relaxed);
    }
    
    // Get statistics
    CircuitBreakerStats get_stats() const;
    
    // Get name
    const std::string& name() const { return name_; }
    
private:
    bool allow_request();
    void on_success();
    void on_failure();
    void transition_to(CircuitState new_state);
    bool should_attempt_reset() const;
    
    std::string name_;
    CircuitBreakerConfig config_;
    std::atomic<CircuitState> state_;
    mutable std::mutex mutex_;
    CircuitBreakerStats stats_;
    std::atomic<int> concurrent_calls_;
};

// Circuit breaker registry for managing multiple circuit breakers
class CircuitBreakerRegistry {
public:
    static CircuitBreakerRegistry& instance();
    
    // Get or create a circuit breaker
    std::shared_ptr<CircuitBreaker> get_or_create(
        const std::string& name,
        const CircuitBreakerConfig& config = CircuitBreakerConfig());
    
    // Get existing circuit breaker
    std::shared_ptr<CircuitBreaker> get(const std::string& name);
    
    // Remove circuit breaker
    void remove(const std::string& name);
    
    // Get all circuit breaker names
    std::vector<std::string> get_names() const;
    
    // Get stats for all circuit breakers
    std::unordered_map<std::string, CircuitBreakerStats> get_all_stats() const;
    
    // Reset all circuit breakers
    void reset_all();
    
private:
    CircuitBreakerRegistry() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> breakers_;
};

// Helper function to wrap a function with circuit breaker
template<typename Func>
auto with_circuit_breaker(const std::string& name,
                          Func&& func,
                          const CircuitBreakerConfig& config = CircuitBreakerConfig()) {
    auto& registry = CircuitBreakerRegistry::instance();
    auto breaker = registry.get_or_create(name, config);
    return breaker->execute(std::forward<Func>(func));
}

// Predefined circuit breaker configurations
namespace configs {
    // Fast failure - for external API calls
    inline CircuitBreakerConfig fast_failure() {
        return CircuitBreakerConfig(3, 2, 30000);  // 3 failures, 30s timeout
    }
    
    // Standard - balanced configuration
    inline CircuitBreakerConfig standard() {
        return CircuitBreakerConfig(5, 2, 60000);  // 5 failures, 60s timeout
    }
    
    // Tolerant - for degraded mode operations
    inline CircuitBreakerConfig tolerant() {
        return CircuitBreakerConfig(10, 3, 120000); // 10 failures, 120s timeout
    }
}

} // namespace resilience
} // namespace brain_ai

#endif // BRAIN_AI_RESILIENCE_CIRCUIT_BREAKER_HPP
