#ifndef BRAIN_AI_LOGGING_LOGGER_HPP
#define BRAIN_AI_LOGGING_LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <memory>
#include <vector>
#include <unordered_map>

namespace brain_ai {
namespace logging {

// Log levels (ordered by severity)
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

// Convert log level to string
inline const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// Log message structure
struct LogMessage {
    LogLevel level;
    std::string timestamp;
    std::string logger_name;
    std::string message;
    std::string file;
    int line;
    std::string function;
    
    std::string format() const;
};

// Abstract log sink
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogMessage& msg) = 0;
    virtual void flush() = 0;
};

// Console sink - writes to stdout/stderr
class ConsoleSink : public LogSink {
public:
    ConsoleSink() = default;
    void write(const LogMessage& msg) override;
    void flush() override;
};

// File sink - writes to file with rotation
class FileSink : public LogSink {
public:
    explicit FileSink(const std::string& filepath, 
                      size_t max_size_mb = 100,
                      size_t max_files = 5);
    ~FileSink() override;
    
    void write(const LogMessage& msg) override;
    void flush() override;
    
private:
    void rotate_if_needed();
    
    std::string filepath_;
    std::ofstream file_;
    size_t max_size_bytes_;
    size_t max_files_;
    size_t current_size_;
    std::mutex mutex_;
};

// Logger class
class Logger {
public:
    explicit Logger(const std::string& name);
    
    // Set log level
    void set_level(LogLevel level) { level_ = level; }
    LogLevel get_level() const { return level_; }
    
    // Add/remove sinks
    void add_sink(std::shared_ptr<LogSink> sink);
    void clear_sinks();
    
    // Check if level is enabled
    bool should_log(LogLevel level) const {
        return level >= level_;
    }
    
    // Log methods
    void log(LogLevel level, const std::string& message,
             const char* file = "", int line = 0, const char* func = "");
    
    void trace(const std::string& message,
               const char* file = "", int line = 0, const char* func = "");
    void debug(const std::string& message,
               const char* file = "", int line = 0, const char* func = "");
    void info(const std::string& message,
              const char* file = "", int line = 0, const char* func = "");
    void warn(const std::string& message,
              const char* file = "", int line = 0, const char* func = "");
    void error(const std::string& message,
               const char* file = "", int line = 0, const char* func = "");
    void fatal(const std::string& message,
               const char* file = "", int line = 0, const char* func = "");
    
    // Flush all sinks
    void flush();
    
private:
    std::string name_;
    LogLevel level_;
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::mutex mutex_;
};

// Logger registry - manages all loggers
class LoggerRegistry {
public:
    static LoggerRegistry& instance();
    
    // Get or create logger
    std::shared_ptr<Logger> get_logger(const std::string& name);
    
    // Set global log level for all loggers
    void set_global_level(LogLevel level);
    
    // Add sink to all loggers
    void add_global_sink(std::shared_ptr<LogSink> sink);
    
    // Flush all loggers
    void flush_all();
    
private:
    LoggerRegistry();
    
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    std::mutex mutex_;
};

// Helper function to get current timestamp
std::string get_timestamp();

// Convenience macros for logging
#define LOG_TRACE(logger, msg) \
    do { \
        const auto& _log_msg = (msg); \
        if ((logger)->should_log(brain_ai::logging::LogLevel::TRACE)) { \
            (logger)->trace(_log_msg, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

#define LOG_DEBUG(logger, msg) \
    do { \
        const auto& _log_msg = (msg); \
        if ((logger)->should_log(brain_ai::logging::LogLevel::DEBUG)) { \
            (logger)->debug(_log_msg, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

#define LOG_INFO(logger, msg) \
    do { \
        const auto& _log_msg = (msg); \
        if ((logger)->should_log(brain_ai::logging::LogLevel::INFO)) { \
            (logger)->info(_log_msg, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

#define LOG_WARN(logger, msg) \
    do { \
        const auto& _log_msg = (msg); \
        if ((logger)->should_log(brain_ai::logging::LogLevel::WARN)) { \
            (logger)->warn(_log_msg, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

#define LOG_ERROR(logger, msg) \
    do { \
        const auto& _log_msg = (msg); \
        if ((logger)->should_log(brain_ai::logging::LogLevel::ERROR)) { \
            (logger)->error(_log_msg, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

#define LOG_FATAL(logger, msg) \
    do { \
        const auto& _log_msg = (msg); \
        if ((logger)->should_log(brain_ai::logging::LogLevel::FATAL)) { \
            (logger)->fatal(_log_msg, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

// Get logger by name
#define GET_LOGGER(name) \
    brain_ai::logging::LoggerRegistry::instance().get_logger(name)

// Common logger names
namespace logger_names {
    constexpr const char* MAIN = "brain_ai.main";
    constexpr const char* EPISODIC = "brain_ai.episodic_buffer";
    constexpr const char* SEMANTIC = "brain_ai.semantic_network";
    constexpr const char* HALLUCINATION = "brain_ai.hallucination_detector";
    constexpr const char* FUSION = "brain_ai.hybrid_fusion";
    constexpr const char* EXPLANATION = "brain_ai.explanation_engine";
    constexpr const char* COGNITIVE = "brain_ai.cognitive_handler";
}

// Initialize logging system with default configuration
void initialize_logging(LogLevel level = LogLevel::INFO,
                        const std::string& log_file = "");

} // namespace logging
} // namespace brain_ai

#endif // BRAIN_AI_LOGGING_LOGGER_HPP
