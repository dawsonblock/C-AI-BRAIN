#include "resilience/circuit_breaker.hpp"
#include <sstream>
#include <iomanip>

namespace brain_ai {
namespace resilience {

// ============================================================================
// CircuitBreakerStats Implementation
// ============================================================================

std::string CircuitBreakerStats::to_json() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"state\": \"" << circuit_state_to_string(state) << "\",\n";
    oss << "  \"total_calls\": " << total_calls << ",\n";
    oss << "  \"successful_calls\": " << successful_calls << ",\n";
    oss << "  \"failed_calls\": " << failed_calls << ",\n";
    oss << "  \"rejected_calls\": " << rejected_calls << ",\n";
    oss << "  \"consecutive_failures\": " << consecutive_failures << ",\n";
    oss << "  \"consecutive_successes\": " << consecutive_successes << ",\n";
    oss << "  \"current_concurrent_calls\": " << current_concurrent_calls << ",\n";
    oss << "  \"last_failure_time\": \"" 
        << std::chrono::system_clock::to_time_t(last_failure_time) << "\",\n";
    oss << "  \"last_state_change_time\": \"" 
        << std::chrono::system_clock::to_time_t(last_state_change_time) << "\"\n";
    oss << "}";
    
    return oss.str();
}

// ============================================================================
// CircuitBreaker Implementation
// ============================================================================

bool CircuitBreaker::allow_request() {
    CircuitState current_state = state_.load(std::memory_order_relaxed);
    
    switch (current_state) {
        case CircuitState::CLOSED:
            // Check concurrent call limit
            if (concurrent_calls_.load() >= config_.max_concurrent_calls) {
                return false;
            }
            return true;
            
        case CircuitState::OPEN:
            // Check if we should attempt reset
            if (should_attempt_reset()) {
                transition_to(CircuitState::HALF_OPEN);
                return true;
            }
            return false;
            
        case CircuitState::HALF_OPEN:
            // Allow one request at a time in half-open state
            return concurrent_calls_.load() == 0;
            
        default:
            return false;
    }
}

void CircuitBreaker::on_success() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stats_.total_calls++;
    stats_.successful_calls++;
    stats_.consecutive_successes++;
    stats_.consecutive_failures = 0;
    
    CircuitState current_state = state_.load(std::memory_order_relaxed);
    
    if (current_state == CircuitState::HALF_OPEN) {
        // Check if we should close the circuit
        if (stats_.consecutive_successes >= config_.success_threshold) {
            transition_to(CircuitState::CLOSED);
        }
    }
}

void CircuitBreaker::on_failure() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stats_.total_calls++;
    stats_.failed_calls++;
    stats_.consecutive_failures++;
    stats_.consecutive_successes = 0;
    stats_.last_failure_time = std::chrono::system_clock::now();
    
    CircuitState current_state = state_.load(std::memory_order_relaxed);
    
    if (current_state == CircuitState::HALF_OPEN) {
        // Any failure in half-open state opens the circuit
        transition_to(CircuitState::OPEN);
    } else if (current_state == CircuitState::CLOSED) {
        // Check if we should open the circuit
        if (stats_.consecutive_failures >= config_.failure_threshold) {
            transition_to(CircuitState::OPEN);
        }
    }
}

void CircuitBreaker::transition_to(CircuitState new_state) {
    // Already holding mutex_
    
    CircuitState old_state = state_.load(std::memory_order_relaxed);
    
    if (old_state == new_state) {
        return;
    }
    
    state_.store(new_state, std::memory_order_relaxed);
    stats_.state = new_state;
    stats_.last_state_change_time = std::chrono::system_clock::now();
    
    // Reset counters on state transition
    if (new_state == CircuitState::CLOSED) {
        stats_.consecutive_failures = 0;
        stats_.consecutive_successes = 0;
    } else if (new_state == CircuitState::HALF_OPEN) {
        stats_.consecutive_successes = 0;
    }
}

bool CircuitBreaker::should_attempt_reset() const {
    // Check if timeout has elapsed since last failure
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats_.last_failure_time).count();
    
    return elapsed >= config_.timeout_ms;
}

void CircuitBreaker::trip() {
    std::lock_guard<std::mutex> lock(mutex_);
    transition_to(CircuitState::OPEN);
    stats_.last_failure_time = std::chrono::system_clock::now();
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    transition_to(CircuitState::CLOSED);
    stats_.consecutive_failures = 0;
    stats_.consecutive_successes = 0;
}

CircuitBreakerStats CircuitBreaker::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

// ============================================================================
// CircuitBreakerRegistry Implementation
// ============================================================================

CircuitBreakerRegistry& CircuitBreakerRegistry::instance() {
    static CircuitBreakerRegistry registry;
    return registry;
}

std::shared_ptr<CircuitBreaker> CircuitBreakerRegistry::get_or_create(
    const std::string& name,
    const CircuitBreakerConfig& config) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = breakers_.find(name);
    if (it != breakers_.end()) {
        return it->second;
    }
    
    // Create new circuit breaker
    auto breaker = std::make_shared<CircuitBreaker>(name, config);
    breakers_[name] = breaker;
    
    return breaker;
}

std::shared_ptr<CircuitBreaker> CircuitBreakerRegistry::get(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = breakers_.find(name);
    if (it == breakers_.end()) {
        return nullptr;
    }
    
    return it->second;
}

void CircuitBreakerRegistry::remove(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    breakers_.erase(name);
}

std::vector<std::string> CircuitBreakerRegistry::get_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    names.reserve(breakers_.size());
    for (const auto& [name, _] : breakers_) {
        names.push_back(name);
    }
    
    return names;
}

std::unordered_map<std::string, CircuitBreakerStats> 
CircuitBreakerRegistry::get_all_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::unordered_map<std::string, CircuitBreakerStats> all_stats;
    for (const auto& [name, breaker] : breakers_) {
        all_stats[name] = breaker->get_stats();
    }
    
    return all_stats;
}

void CircuitBreakerRegistry::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [_, breaker] : breakers_) {
        breaker->reset();
    }
}

} // namespace resilience
} // namespace brain_ai
