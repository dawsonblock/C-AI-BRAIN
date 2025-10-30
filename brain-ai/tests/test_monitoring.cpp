#include "monitoring/metrics.hpp"
#include "monitoring/health.hpp"
#include <thread>
#include <chrono>
#include <iostream>

using namespace brain_ai::monitoring;

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

void test_counter_basic() {
    Counter counter;
    EXPECT_EQ(counter.value(), 0);
    
    counter.increment();
    EXPECT_EQ(counter.value(), 1);
    
    counter.increment(5);
    EXPECT_EQ(counter.value(), 6);
    
    counter.reset();
    EXPECT_EQ(counter.value(), 0);
}

void test_counter_thread_safety() {
    Counter counter;
    const int num_threads = 10;
    const int increments_per_thread = 1000;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                counter.increment();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(counter.value(), num_threads * increments_per_thread);
}

void test_gauge_basic() {
    Gauge gauge;
    EXPECT_EQ(gauge.value(), 0.0);
    
    gauge.set(42.5);
    EXPECT_NEAR(gauge.value(), 42.5, 0.01);
    
    gauge.increment(10.5);
    EXPECT_NEAR(gauge.value(), 53.0, 0.01);
    
    gauge.decrement(3.0);
    EXPECT_NEAR(gauge.value(), 50.0, 0.01);
}

void test_histogram_basic() {
    Histogram histogram(100);
    
    // Add some values
    for (int i = 1; i <= 10; ++i) {
        histogram.observe(static_cast<double>(i));
    }
    
    auto stats = histogram.get_statistics();
    
    EXPECT_EQ(stats.count, 10);
    EXPECT_NEAR(stats.min, 1.0, 0.01);
    EXPECT_NEAR(stats.max, 10.0, 0.01);
    EXPECT_NEAR(stats.mean, 5.5, 0.01);
    EXPECT_NEAR(stats.p50, 5.5, 0.5); // Median around 5.5
}

void test_histogram_percentiles() {
    Histogram histogram(1000);
    
    // Add values 1-100
    for (int i = 1; i <= 100; ++i) {
        histogram.observe(static_cast<double>(i));
    }
    
    auto stats = histogram.get_statistics();
    
    EXPECT_NEAR(stats.p50, 50.0, 2.0);  // Median ~50
    EXPECT_NEAR(stats.p95, 95.0, 2.0);  // 95th percentile ~95
    EXPECT_NEAR(stats.p99, 99.0, 2.0);  // 99th percentile ~99
}

void test_timer_basic() {
    Timer timer;
    
    // Record some durations (microseconds)
    timer.record(1000);  // 1ms
    timer.record(2000);  // 2ms
    timer.record(3000);  // 3ms
    
    auto stats = timer.get_statistics();
    
    EXPECT_EQ(stats.count, 3);
    EXPECT_NEAR(stats.min, 1000.0, 1.0);
    EXPECT_NEAR(stats.max, 3000.0, 1.0);
    EXPECT_NEAR(stats.mean, 2000.0, 1.0);
}

void test_timer_scoped() {
    Timer timer;
    
    {
        Timer::ScopedTimer scoped(timer);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto stats = timer.get_statistics();
    EXPECT_EQ(stats.count, 1);
    EXPECT_TRUE(stats.mean >= 10000.0); // At least 10ms in microseconds
}

void test_metrics_registry() {
    auto& registry = MetricsRegistry::instance();
    
    // Test counter
    auto& counter = registry.get_counter("test_counter");
    counter.increment(5);
    EXPECT_EQ(counter.value(), 5);
    
    // Test gauge
    auto& gauge = registry.get_gauge("test_gauge");
    gauge.set(123.45);
    EXPECT_NEAR(gauge.value(), 123.45, 0.01);
    
    // Test histogram
    auto& histogram = registry.get_histogram("test_histogram");
    histogram.observe(10.0);
    auto stats = histogram.get_statistics();
    EXPECT_EQ(stats.count, 1);
    
    // Test timer
    auto& timer = registry.get_timer("test_timer");
    timer.record(5000);
    auto timer_stats = timer.get_statistics();
    EXPECT_EQ(timer_stats.count, 1);
}

void test_metrics_export() {
    auto& registry = MetricsRegistry::instance();
    
    registry.get_counter("export_counter").increment(10);
    registry.get_gauge("export_gauge").set(99.9);
    
    std::string json = registry.export_metrics();
    
    // Check that JSON contains expected fields
    EXPECT_TRUE(json.find("counters") != std::string::npos);
    EXPECT_TRUE(json.find("gauges") != std::string::npos);
    EXPECT_TRUE(json.find("histograms") != std::string::npos);
    EXPECT_TRUE(json.find("timers") != std::string::npos);
}

void test_health_check_result() {
    auto result = create_health_result("test_component", 
                                       HealthStatus::HEALTHY,
                                       "All systems operational");
    
    EXPECT_EQ(result.component_name, "test_component");
    EXPECT_TRUE(result.status == HealthStatus::HEALTHY);
    EXPECT_EQ(result.message, "All systems operational");
    
    std::string json = result.to_json();
    EXPECT_TRUE(json.find("test_component") != std::string::npos);
    EXPECT_TRUE(json.find("HEALTHY") != std::string::npos);
}

void test_health_check_execution() {
    HealthCheck check("test_check", []() {
        return create_health_result("test", HealthStatus::HEALTHY, "OK");
    });
    
    auto result = check.execute();
    
    EXPECT_TRUE(result.status == HealthStatus::HEALTHY);
    EXPECT_EQ(result.message, "OK");
    EXPECT_EQ(check.consecutive_failures(), 0);
}

void test_health_check_failure_tracking() {
    int call_count = 0;
    HealthCheck check("failing_check", [&call_count]() {
        call_count++;
        return create_health_result("test", HealthStatus::UNHEALTHY, "Failed");
    });
    
    // Execute multiple times
    for (int i = 0; i < 3; ++i) {
        check.execute();
    }
    
    EXPECT_EQ(call_count, 3);
    EXPECT_EQ(check.consecutive_failures(), 3);
}

void test_system_health_aggregation() {
    SystemHealth health;
    
    health.component_results.push_back(
        create_health_result("c1", HealthStatus::HEALTHY, "OK"));
    health.component_results.push_back(
        create_health_result("c2", HealthStatus::HEALTHY, "OK"));
    
    health.compute_overall_status();
    
    EXPECT_TRUE(health.overall_status == HealthStatus::HEALTHY);
}

void test_system_health_degraded() {
    SystemHealth health;
    
    health.component_results.push_back(
        create_health_result("c1", HealthStatus::HEALTHY, "OK"));
    health.component_results.push_back(
        create_health_result("c2", HealthStatus::DEGRADED, "Warning"));
    
    health.compute_overall_status();
    
    EXPECT_TRUE(health.overall_status == HealthStatus::DEGRADED);
}

void test_system_health_unhealthy() {
    SystemHealth health;
    
    health.component_results.push_back(
        create_health_result("c1", HealthStatus::HEALTHY, "OK"));
    health.component_results.push_back(
        create_health_result("c2", HealthStatus::UNHEALTHY, "Failed"));
    
    health.compute_overall_status();
    
    EXPECT_TRUE(health.overall_status == HealthStatus::UNHEALTHY);
}

void test_health_registry() {
    auto& registry = HealthCheckRegistry::instance();
    
    registry.register_check("registry_test", []() {
        return create_health_result("test", HealthStatus::HEALTHY, "OK");
    });
    
    auto names = registry.get_check_names();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "registry_test") != names.end());
    
    auto result = registry.check_one("registry_test");
    EXPECT_TRUE(result.status == HealthStatus::HEALTHY);
    
    registry.unregister_check("registry_test");
}

void test_predefined_health_checks() {
    // Memory check
    auto memory_result = check_memory_health();
    EXPECT_EQ(memory_result.component_name, "memory");
    EXPECT_TRUE(!memory_result.details.empty());
    
    // Disk check
    auto disk_result = check_disk_health();
    EXPECT_EQ(disk_result.component_name, "disk");
    EXPECT_TRUE(!disk_result.details.empty());
    
    // Thread check
    auto thread_result = check_thread_health();
    EXPECT_EQ(thread_result.component_name, "threads");
    EXPECT_TRUE(!thread_result.details.empty());
}

int main() {
    std::cout << "Running Monitoring Tests...\n";
    std::cout << "============================================================\n\n";
    
    // Counter tests
    run_test("Counter basic operations", test_counter_basic);
    run_test("Counter thread safety", test_counter_thread_safety);
    
    // Gauge tests
    run_test("Gauge basic operations", test_gauge_basic);
    
    // Histogram tests
    run_test("Histogram basic statistics", test_histogram_basic);
    run_test("Histogram percentile calculation", test_histogram_percentiles);
    
    // Timer tests
    run_test("Timer basic operations", test_timer_basic);
    run_test("Timer scoped timing", test_timer_scoped);
    
    // Registry tests
    run_test("Metrics registry operations", test_metrics_registry);
    run_test("Metrics JSON export", test_metrics_export);
    
    // Health check tests
    run_test("Health check result creation", test_health_check_result);
    run_test("Health check execution", test_health_check_execution);
    run_test("Health check failure tracking", test_health_check_failure_tracking);
    run_test("System health aggregation (healthy)", test_system_health_aggregation);
    run_test("System health aggregation (degraded)", test_system_health_degraded);
    run_test("System health aggregation (unhealthy)", test_system_health_unhealthy);
    run_test("Health check registry", test_health_registry);
    run_test("Predefined health checks", test_predefined_health_checks);
    
    std::cout << "\n============================================================\n";
    std::cout << "Monitoring Tests Complete\n";
    std::cout << "============================================================\n";
    
    return 0;
}
