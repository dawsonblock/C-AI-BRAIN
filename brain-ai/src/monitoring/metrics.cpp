#include "monitoring/metrics.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace brain_ai {
namespace monitoring {

// ============================================================================
// Histogram Implementation
// ============================================================================

Histogram::Histogram(size_t max_samples)
    : max_samples_(max_samples), total_count_(0), sum_(0.0) {
    samples_.reserve(max_samples);
}

void Histogram::observe(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (samples_.size() >= max_samples_) {
        // Rolling window: remove oldest sample
        samples_.erase(samples_.begin());
    }
    
    samples_.push_back(value);
    total_count_++;
    sum_ += value;
}

Statistics Histogram::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Statistics stats;
    
    if (samples_.empty()) {
        return stats;
    }
    
    // Sort samples for percentile calculation
    std::vector<double> sorted_samples = samples_;
    std::sort(sorted_samples.begin(), sorted_samples.end());
    
    stats.count = total_count_;
    stats.sum = sum_;
    stats.min = sorted_samples.front();
    stats.max = sorted_samples.back();
    stats.mean = sum_ / static_cast<double>(samples_.size());
    
    // Calculate percentiles
    auto percentile = [&sorted_samples](double p) -> double {
        size_t n = sorted_samples.size();
        double index = p * (n - 1);
        size_t lower = static_cast<size_t>(std::floor(index));
        size_t upper = static_cast<size_t>(std::ceil(index));
        
        if (lower == upper) {
            return sorted_samples[lower];
        }
        
        double weight = index - lower;
        return sorted_samples[lower] * (1.0 - weight) + sorted_samples[upper] * weight;
    };
    
    stats.p50 = percentile(0.50);
    stats.p95 = percentile(0.95);
    stats.p99 = percentile(0.99);
    
    return stats;
}

void Histogram::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    samples_.clear();
    total_count_ = 0;
    sum_ = 0.0;
}

// ============================================================================
// Timer Implementation
// ============================================================================

Timer::Timer() : histogram_(10000) {} // Store up to 10k measurements

void Timer::record(int64_t duration_us) {
    histogram_.observe(static_cast<double>(duration_us));
}

Statistics Timer::get_statistics() const {
    return histogram_.get_statistics();
}

void Timer::reset() {
    histogram_.reset();
}

// ============================================================================
// MetricsRegistry Implementation
// ============================================================================

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry registry;
    return registry;
}

Counter& MetricsRegistry::get_counter(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return counters_[name];
}

Gauge& MetricsRegistry::get_gauge(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return gauges_[name];
}

Histogram& MetricsRegistry::get_histogram(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = histograms_.find(name);
    if (it == histograms_.end()) {
        // Use try_emplace to construct Histogram in-place without copy/move
        it = histograms_.try_emplace(name, 10000).first;
    }
    return it->second;
}

Timer& MetricsRegistry::get_timer(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = timers_.find(name);
    if (it == timers_.end()) {
        // Use try_emplace to construct Timer in-place without copy/move
        it = timers_.try_emplace(name).first;
    }
    return it->second;
}

std::vector<std::string> MetricsRegistry::get_counter_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(counters_.size());
    for (const auto& [name, _] : counters_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> MetricsRegistry::get_gauge_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(gauges_.size());
    for (const auto& [name, _] : gauges_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> MetricsRegistry::get_histogram_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(histograms_.size());
    for (const auto& [name, _] : histograms_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> MetricsRegistry::get_timer_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(timers_.size());
    for (const auto& [name, _] : timers_) {
        names.push_back(name);
    }
    return names;
}

std::string MetricsRegistry::export_metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"counters\": {\n";
    bool first = true;
    for (const auto& [name, counter] : counters_) {
        if (!first) oss << ",\n";
        oss << "    \"" << name << "\": " << counter.value();
        first = false;
    }
    oss << "\n  },\n";
    
    oss << "  \"gauges\": {\n";
    first = true;
    for (const auto& [name, gauge] : gauges_) {
        if (!first) oss << ",\n";
        oss << "    \"" << name << "\": " << std::fixed << std::setprecision(2) << gauge.value();
        first = false;
    }
    oss << "\n  },\n";
    
    oss << "  \"histograms\": {\n";
    first = true;
    for (const auto& [name, histogram] : histograms_) {
        if (!first) oss << ",\n";
        auto stats = histogram.get_statistics();
        oss << "    \"" << name << "\": {\n";
        oss << "      \"count\": " << stats.count << ",\n";
        oss << "      \"sum\": " << std::fixed << std::setprecision(2) << stats.sum << ",\n";
        oss << "      \"min\": " << std::fixed << std::setprecision(2) << stats.min << ",\n";
        oss << "      \"max\": " << std::fixed << std::setprecision(2) << stats.max << ",\n";
        oss << "      \"mean\": " << std::fixed << std::setprecision(2) << stats.mean << ",\n";
        oss << "      \"p50\": " << std::fixed << std::setprecision(2) << stats.p50 << ",\n";
        oss << "      \"p95\": " << std::fixed << std::setprecision(2) << stats.p95 << ",\n";
        oss << "      \"p99\": " << std::fixed << std::setprecision(2) << stats.p99 << "\n";
        oss << "    }";
        first = false;
    }
    oss << "\n  },\n";
    
    oss << "  \"timers\": {\n";
    first = true;
    for (const auto& [name, timer] : timers_) {
        if (!first) oss << ",\n";
        auto stats = timer.get_statistics();
        oss << "    \"" << name << "\": {\n";
        oss << "      \"count\": " << stats.count << ",\n";
        oss << "      \"min_us\": " << std::fixed << std::setprecision(2) << stats.min << ",\n";
        oss << "      \"max_us\": " << std::fixed << std::setprecision(2) << stats.max << ",\n";
        oss << "      \"mean_us\": " << std::fixed << std::setprecision(2) << stats.mean << ",\n";
        oss << "      \"p50_us\": " << std::fixed << std::setprecision(2) << stats.p50 << ",\n";
        oss << "      \"p95_us\": " << std::fixed << std::setprecision(2) << stats.p95 << ",\n";
        oss << "      \"p99_us\": " << std::fixed << std::setprecision(2) << stats.p99 << "\n";
        oss << "    }";
        first = false;
    }
    oss << "\n  }\n";
    
    oss << "}\n";
    
    return oss.str();
}

void MetricsRegistry::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [_, counter] : counters_) {
        counter.reset();
    }
    
    for (auto& [_, histogram] : histograms_) {
        histogram.reset();
    }
    
    for (auto& [_, timer] : timers_) {
        timer.reset();
    }
    
    // Gauges are not reset as they represent current values
}

} // namespace monitoring
} // namespace brain_ai
