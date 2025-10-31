#ifndef BRAIN_AI_MONITORING_METRICS_HPP
#define BRAIN_AI_MONITORING_METRICS_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>

namespace brain_ai {
namespace monitoring {

// Time point type
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::microseconds;

// Metric types
enum class MetricType {
    COUNTER,      // Monotonically increasing value
    GAUGE,        // Current value that can go up or down
    HISTOGRAM,    // Distribution of values
    TIMER         // Duration measurements
};

// Statistical summary for histograms
struct Statistics {
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double p50 = 0.0;   // Median
    double p95 = 0.0;
    double p99 = 0.0;
    size_t count = 0;
    double sum = 0.0;
};

// Counter metric - monotonically increasing
class Counter {
public:
    Counter() : value_(0) {}
    
    void increment(int64_t delta = 1) {
        value_.fetch_add(delta, std::memory_order_relaxed);
    }
    
    int64_t value() const {
        return value_.load(std::memory_order_relaxed);
    }
    
    void reset() {
        value_.store(0, std::memory_order_relaxed);
    }
    
private:
    std::atomic<int64_t> value_;
};

// Gauge metric - can increase or decrease
class Gauge {
public:
    Gauge() : value_(0.0) {}
    
    void set(double val) {
        value_.store(val, std::memory_order_relaxed);
    }
    
    void increment(double delta = 1.0) {
        // fetch_add is not available for float/double, so use a CAS loop
        double current = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(current, current + delta, std::memory_order_relaxed)) {
            // The loop continues if another thread modified 'current'
        }
    }
    
    void decrement(double delta = 1.0) {
        // fetch_sub is not available for float/double, so use a CAS loop
        double current = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(current, current - delta, std::memory_order_relaxed)) {
            // The loop continues if another thread modified 'current'
        }
    }
    
    double value() const {
        return value_.load(std::memory_order_relaxed);
    }
    
private:
    std::atomic<double> value_;
};

// Histogram - tracks distribution of values
class Histogram {
public:
    explicit Histogram(size_t max_samples = 10000);
    
    void observe(double value);
    Statistics get_statistics() const;
    void reset();
    
private:
    mutable std::mutex mutex_;
    std::vector<double> samples_;
    size_t max_samples_;
    size_t total_count_;
    double sum_;
};

// Timer - measures duration
class Timer {
public:
    Timer();
    
    // Record a duration in microseconds
    void record(int64_t duration_us);
    
    // Get statistics
    Statistics get_statistics() const;
    
    // Reset all measurements
    void reset();
    
    // RAII timer helper
    class ScopedTimer {
    public:
        explicit ScopedTimer(Timer& timer) 
            : timer_(timer), start_(std::chrono::steady_clock::now()) {}
        
        ~ScopedTimer() {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start_).count();
            timer_.record(duration);
        }
        
    private:
        Timer& timer_;
        TimePoint start_;
    };
    
private:
    Histogram histogram_;
};

// Metrics registry - central storage for all metrics
class MetricsRegistry {
public:
    static MetricsRegistry& instance();
    
    // Get or create metrics
    Counter& get_counter(const std::string& name);
    Gauge& get_gauge(const std::string& name);
    Histogram& get_histogram(const std::string& name);
    Timer& get_timer(const std::string& name);
    
    // Get all metric names by type
    std::vector<std::string> get_counter_names() const;
    std::vector<std::string> get_gauge_names() const;
    std::vector<std::string> get_histogram_names() const;
    std::vector<std::string> get_timer_names() const;
    
    // Export all metrics as JSON-like string
    std::string export_metrics() const;
    
    // Reset all metrics
    void reset_all();
    
private:
    MetricsRegistry() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Counter> counters_;
    std::unordered_map<std::string, Gauge> gauges_;
    std::unordered_map<std::string, Histogram> histograms_;
    std::unordered_map<std::string, Timer> timers_;
};

// Convenience macros for common metrics
#define METRICS_COUNTER_INC(name) \
    brain_ai::monitoring::MetricsRegistry::instance().get_counter(name).increment()

#define METRICS_COUNTER_ADD(name, delta) \
    brain_ai::monitoring::MetricsRegistry::instance().get_counter(name).increment(delta)

#define METRICS_GAUGE_SET(name, value) \
    brain_ai::monitoring::MetricsRegistry::instance().get_gauge(name).set(value)

#define METRICS_HISTOGRAM_OBSERVE(name, value) \
    brain_ai::monitoring::MetricsRegistry::instance().get_histogram(name).observe(value)

#define METRICS_TIMER_SCOPE(name) \
    brain_ai::monitoring::Timer::ScopedTimer _scoped_timer_##__COUNTER__( \
        brain_ai::monitoring::MetricsRegistry::instance().get_timer(name))

// Predefined metric names
namespace metric_names {
    // Query processing
    constexpr const char* QUERIES_TOTAL = "queries_total";
    constexpr const char* QUERIES_FAILED = "queries_failed";
    constexpr const char* QUERY_LATENCY = "query_latency_us";
    
    // Episodic buffer
    constexpr const char* EPISODES_STORED = "episodes_stored";
    constexpr const char* EPISODES_RETRIEVED = "episodes_retrieved";
    constexpr const char* EPISODIC_CACHE_HITS = "episodic_cache_hits";
    constexpr const char* EPISODIC_CACHE_MISSES = "episodic_cache_misses";
    
    // Semantic network
    constexpr const char* SEMANTIC_ACTIVATIONS = "semantic_activations";
    constexpr const char* SEMANTIC_NODES = "semantic_nodes_count";
    constexpr const char* SEMANTIC_EDGES = "semantic_edges_count";
    
    // Hallucination detection
    constexpr const char* HALLUCINATIONS_DETECTED = "hallucinations_detected";
    constexpr const char* VALIDATION_CONFIDENCE = "validation_confidence";
    
    // System health
    constexpr const char* MEMORY_USAGE_MB = "memory_usage_mb";
    constexpr const char* CPU_USAGE_PERCENT = "cpu_usage_percent";
    constexpr const char* THREAD_COUNT = "thread_count";
    
    // Performance
    constexpr const char* QPS_CURRENT = "qps_current";
    constexpr const char* THROUGHPUT_TOTAL = "throughput_total";
}

} // namespace monitoring
} // namespace brain_ai

#endif // BRAIN_AI_MONITORING_METRICS_HPP
