#ifndef BRAIN_AI_MONITORING_HEALTH_HPP
#define BRAIN_AI_MONITORING_HEALTH_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <chrono>
#include <memory>

namespace brain_ai {
namespace monitoring {

// Health status enumeration
enum class HealthStatus {
    HEALTHY,      // Component is functioning normally
    DEGRADED,     // Component is functioning but with issues
    UNHEALTHY,    // Component is not functioning
    UNKNOWN       // Status cannot be determined
};

// Convert health status to string
inline const char* health_status_to_string(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::UNHEALTHY: return "UNHEALTHY";
        case HealthStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

// Health check result
struct HealthCheckResult {
    std::string component_name;
    HealthStatus status;
    std::string message;
    std::unordered_map<std::string, std::string> details;
    std::chrono::system_clock::time_point timestamp;
    int64_t check_duration_ms;
    
    HealthCheckResult()
        : status(HealthStatus::UNKNOWN)
        , timestamp(std::chrono::system_clock::now())
        , check_duration_ms(0) {}
    
    // Format as JSON-like string
    std::string to_json() const;
};

// Health check function type
using HealthCheckFunction = std::function<HealthCheckResult()>;

// Individual health check
class HealthCheck {
public:
    HealthCheck(const std::string& name,
                HealthCheckFunction check_func,
                int timeout_ms = 5000)
        : name_(name)
        , check_func_(check_func)
        , timeout_ms_(timeout_ms)
        , last_result_()
        , consecutive_failures_(0) {}
    
    // Execute health check
    HealthCheckResult execute();
    
    // Get last result (cached)
    HealthCheckResult get_last_result() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_result_;
    }
    
    const std::string& name() const { return name_; }
    int consecutive_failures() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return consecutive_failures_;
    }
    
private:
    std::string name_;
    HealthCheckFunction check_func_;
    int timeout_ms_;
    mutable std::mutex mutex_;
    HealthCheckResult last_result_;
    int consecutive_failures_;
};

// System-wide health status
struct SystemHealth {
    HealthStatus overall_status;
    std::vector<HealthCheckResult> component_results;
    std::chrono::system_clock::time_point timestamp;
    
    SystemHealth()
        : overall_status(HealthStatus::UNKNOWN)
        , timestamp(std::chrono::system_clock::now()) {}
    
    // Format as JSON string
    std::string to_json() const;
    
    // Determine overall status from components
    void compute_overall_status();
};

// Health check registry
class HealthCheckRegistry {
public:
    static HealthCheckRegistry& instance();
    
    // Register a health check
    void register_check(const std::string& name,
                       HealthCheckFunction check_func,
                       int timeout_ms = 5000);
    
    // Execute all health checks
    SystemHealth check_all();
    
    // Execute specific health check
    HealthCheckResult check_one(const std::string& name);
    
    // Get all registered check names
    std::vector<std::string> get_check_names() const;
    
    // Remove a health check
    void unregister_check(const std::string& name);
    
    // Clear all checks
    void clear_all();
    
private:
    HealthCheckRegistry() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<HealthCheck>> checks_;
};

// Predefined health checks for Brain-AI components

// Episodic buffer health check
HealthCheckResult check_episodic_buffer_health();

// Semantic network health check
HealthCheckResult check_semantic_network_health();

// Memory usage health check
HealthCheckResult check_memory_health();

// Thread health check
HealthCheckResult check_thread_health();

// Disk space health check
HealthCheckResult check_disk_health();

// Initialize default health checks
void initialize_default_health_checks();

// Helper function to create a health check result
inline HealthCheckResult create_health_result(const std::string& component,
                                              HealthStatus status,
                                              const std::string& message) {
    HealthCheckResult result;
    result.component_name = component;
    result.status = status;
    result.message = message;
    result.timestamp = std::chrono::system_clock::now();
    return result;
}

} // namespace monitoring
} // namespace brain_ai

#endif // BRAIN_AI_MONITORING_HEALTH_HPP
