#include "monitoring/health.hpp"
#include "monitoring/metrics.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <future>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace brain_ai {
namespace monitoring {

// ============================================================================
// HealthCheckResult Implementation
// ============================================================================

std::string HealthCheckResult::to_json() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"component\": \"" << component_name << "\",\n";
    oss << "  \"status\": \"" << health_status_to_string(status) << "\",\n";
    oss << "  \"message\": \"" << message << "\",\n";
    oss << "  \"check_duration_ms\": " << check_duration_ms << ",\n";
    oss << "  \"timestamp\": \"" << std::chrono::system_clock::to_time_t(timestamp) << "\"";
    
    if (!details.empty()) {
        oss << ",\n  \"details\": {\n";
        bool first = true;
        for (const auto& [key, value] : details) {
            if (!first) oss << ",\n";
            oss << "    \"" << key << "\": \"" << value << "\"";
            first = false;
        }
        oss << "\n  }";
    }
    
    oss << "\n}";
    
    return oss.str();
}

// ============================================================================
// HealthCheck Implementation
// ============================================================================

HealthCheckResult HealthCheck::execute() {
    auto start = std::chrono::steady_clock::now();
    
    HealthCheckResult result;
    result.component_name = name_;
    result.timestamp = std::chrono::system_clock::now();
    
    try {
        // Execute with timeout
        auto future = std::async(std::launch::async, check_func_);
        
        if (future.wait_for(std::chrono::milliseconds(timeout_ms_)) == 
            std::future_status::timeout) {
            result.status = HealthStatus::UNHEALTHY;
            result.message = "Health check timed out after " + 
                           std::to_string(timeout_ms_) + "ms";
        } else {
            result = future.get();
        }
    } catch (const std::exception& e) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Health check failed: " + std::string(e.what());
    }
    
    auto end = std::chrono::steady_clock::now();
    result.check_duration_ms = 
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Update cached result
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_result_ = result;
        
        if (result.status == HealthStatus::UNHEALTHY) {
            consecutive_failures_++;
        } else {
            consecutive_failures_ = 0;
        }
    }
    
    return result;
}

// ============================================================================
// SystemHealth Implementation
// ============================================================================

std::string SystemHealth::to_json() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"overall_status\": \"" << health_status_to_string(overall_status) << "\",\n";
    oss << "  \"timestamp\": \"" << std::chrono::system_clock::to_time_t(timestamp) << "\",\n";
    oss << "  \"components\": [\n";
    
    for (size_t i = 0; i < component_results.size(); ++i) {
        if (i > 0) oss << ",\n";
        oss << "    " << component_results[i].to_json();
    }
    
    oss << "\n  ]\n";
    oss << "}";
    
    return oss.str();
}

void SystemHealth::compute_overall_status() {
    if (component_results.empty()) {
        overall_status = HealthStatus::UNKNOWN;
        return;
    }
    
    bool has_unhealthy = false;
    bool has_degraded = false;
    
    for (const auto& result : component_results) {
        if (result.status == HealthStatus::UNHEALTHY) {
            has_unhealthy = true;
        } else if (result.status == HealthStatus::DEGRADED) {
            has_degraded = true;
        }
    }
    
    if (has_unhealthy) {
        overall_status = HealthStatus::UNHEALTHY;
    } else if (has_degraded) {
        overall_status = HealthStatus::DEGRADED;
    } else {
        overall_status = HealthStatus::HEALTHY;
    }
}

// ============================================================================
// HealthCheckRegistry Implementation
// ============================================================================

HealthCheckRegistry& HealthCheckRegistry::instance() {
    static HealthCheckRegistry registry;
    return registry;
}

void HealthCheckRegistry::register_check(const std::string& name,
                                        HealthCheckFunction check_func,
                                        int timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    checks_[name] = std::make_unique<HealthCheck>(name, check_func, timeout_ms);
}

SystemHealth HealthCheckRegistry::check_all() {
    std::vector<std::unique_ptr<HealthCheck>*> checks_to_run;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [_, check] : checks_) {
            checks_to_run.push_back(&check);
        }
    }
    
    SystemHealth health;
    health.timestamp = std::chrono::system_clock::now();
    
    for (auto* check_ptr : checks_to_run) {
        health.component_results.push_back((*check_ptr)->execute());
    }
    
    health.compute_overall_status();
    
    return health;
}

HealthCheckResult HealthCheckRegistry::check_one(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = checks_.find(name);
    if (it == checks_.end()) {
        return create_health_result(name, HealthStatus::UNKNOWN, 
                                    "Health check not found");
    }
    
    return it->second->execute();
}

std::vector<std::string> HealthCheckRegistry::get_check_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    names.reserve(checks_.size());
    for (const auto& [name, _] : checks_) {
        names.push_back(name);
    }
    
    return names;
}

void HealthCheckRegistry::unregister_check(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    checks_.erase(name);
}

void HealthCheckRegistry::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    checks_.clear();
}

// ============================================================================
// Predefined Health Checks
// ============================================================================

HealthCheckResult check_episodic_buffer_health() {
    auto result = create_health_result("episodic_buffer", HealthStatus::HEALTHY, 
                                      "Episodic buffer operational");
    
    auto& registry = MetricsRegistry::instance();
    auto& counter = registry.get_counter(metric_names::EPISODES_STORED);
    
    int64_t count = counter.value();
    result.details["episodes_stored"] = std::to_string(count);
    
    if (count == 0) {
        result.status = HealthStatus::DEGRADED;
        result.message = "No episodes stored yet";
    }
    
    return result;
}

HealthCheckResult check_semantic_network_health() {
    auto result = create_health_result("semantic_network", HealthStatus::HEALTHY,
                                      "Semantic network operational");
    
    auto& registry = MetricsRegistry::instance();
    auto& nodes_gauge = registry.get_gauge(metric_names::SEMANTIC_NODES);
    
    double nodes = nodes_gauge.value();
    result.details["node_count"] = std::to_string(static_cast<int>(nodes));
    
    if (nodes == 0) {
        result.status = HealthStatus::DEGRADED;
        result.message = "No semantic nodes initialized";
    }
    
    return result;
}

HealthCheckResult check_memory_health() {
    auto result = create_health_result("memory", HealthStatus::HEALTHY,
                                      "Memory usage within limits");
    
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        result.status = HealthStatus::UNKNOWN;
        result.message = "Failed to get memory info";
        return result;
    }
    
    unsigned long total_ram = info.totalram * info.mem_unit / (1024 * 1024); // MB
    unsigned long free_ram = info.freeram * info.mem_unit / (1024 * 1024);   // MB
    unsigned long used_ram = total_ram - free_ram;
    
    double usage_percent = (static_cast<double>(used_ram) / total_ram) * 100.0;
    
    result.details["total_mb"] = std::to_string(total_ram);
    result.details["used_mb"] = std::to_string(used_ram);
    result.details["free_mb"] = std::to_string(free_ram);
    result.details["usage_percent"] = std::to_string(static_cast<int>(usage_percent));
    
    if (usage_percent > 90.0) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Memory usage critical: " + std::to_string(static_cast<int>(usage_percent)) + "%";
    } else if (usage_percent > 80.0) {
        result.status = HealthStatus::DEGRADED;
        result.message = "Memory usage high: " + std::to_string(static_cast<int>(usage_percent)) + "%";
    } else {
        result.message = "Memory usage normal: " + std::to_string(static_cast<int>(usage_percent)) + "%";
    }
    
    return result;
}

HealthCheckResult check_thread_health() {
    auto result = create_health_result("threads", HealthStatus::HEALTHY,
                                      "Thread count within limits");
    
    // Count threads in current process
    std::ifstream stat_file("/proc/self/stat");
    if (!stat_file.is_open()) {
        result.status = HealthStatus::UNKNOWN;
        result.message = "Failed to read thread info";
        return result;
    }
    
    std::string line;
    std::getline(stat_file, line);
    
    // Thread count is the 20th field
    std::istringstream iss(line);
    std::string field;
    int thread_count = 0;
    for (int i = 0; i < 20; ++i) {
        iss >> field;
    }
    thread_count = std::stoi(field);
    
    result.details["thread_count"] = std::to_string(thread_count);
    
    if (thread_count > 1000) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Thread count critical: " + std::to_string(thread_count);
    } else if (thread_count > 500) {
        result.status = HealthStatus::DEGRADED;
        result.message = "Thread count high: " + std::to_string(thread_count);
    } else {
        result.message = "Thread count normal: " + std::to_string(thread_count);
    }
    
    return result;
}

HealthCheckResult check_disk_health() {
    auto result = create_health_result("disk", HealthStatus::HEALTHY,
                                      "Disk space available");
    
    struct statvfs stat;
    if (statvfs(".", &stat) != 0) {
        result.status = HealthStatus::UNKNOWN;
        result.message = "Failed to get disk info";
        return result;
    }
    
    unsigned long total_space = (stat.f_blocks * stat.f_frsize) / (1024 * 1024); // MB
    unsigned long free_space = (stat.f_bavail * stat.f_frsize) / (1024 * 1024);  // MB
    unsigned long used_space = total_space - free_space;
    
    double usage_percent = (static_cast<double>(used_space) / total_space) * 100.0;
    
    result.details["total_mb"] = std::to_string(total_space);
    result.details["used_mb"] = std::to_string(used_space);
    result.details["free_mb"] = std::to_string(free_space);
    result.details["usage_percent"] = std::to_string(static_cast<int>(usage_percent));
    
    if (usage_percent > 95.0) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Disk usage critical: " + std::to_string(static_cast<int>(usage_percent)) + "%";
    } else if (usage_percent > 85.0) {
        result.status = HealthStatus::DEGRADED;
        result.message = "Disk usage high: " + std::to_string(static_cast<int>(usage_percent)) + "%";
    } else {
        result.message = "Disk usage normal: " + std::to_string(static_cast<int>(usage_percent)) + "%";
    }
    
    return result;
}

void initialize_default_health_checks() {
    auto& registry = HealthCheckRegistry::instance();
    
    registry.register_check("episodic_buffer", check_episodic_buffer_health);
    registry.register_check("semantic_network", check_semantic_network_health);
    registry.register_check("memory", check_memory_health);
    registry.register_check("threads", check_thread_health);
    registry.register_check("disk", check_disk_health);
}

} // namespace monitoring
} // namespace brain_ai
