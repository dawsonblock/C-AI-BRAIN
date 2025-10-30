#include "logging/logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <filesystem>

namespace brain_ai {
namespace logging {

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
    localtime_r(&now_time_t, &tm_buf);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    
    return oss.str();
}

// ============================================================================
// LogMessage Implementation
// ============================================================================

std::string LogMessage::format() const {
    std::ostringstream oss;
    
    oss << "[" << timestamp << "] ";
    oss << "[" << log_level_to_string(level) << "] ";
    oss << "[" << logger_name << "] ";
    
    if (!function.empty()) {
        oss << "[" << function << "] ";
    }
    
    oss << message;
    
    if (!file.empty() && line > 0) {
        oss << " (" << file << ":" << line << ")";
    }
    
    return oss.str();
}

// ============================================================================
// ConsoleSink Implementation
// ============================================================================

void ConsoleSink::write(const LogMessage& msg) {
    std::ostream& out = (msg.level >= LogLevel::ERROR) ? std::cerr : std::cout;
    out << msg.format() << std::endl;
}

void ConsoleSink::flush() {
    std::cout.flush();
    std::cerr.flush();
}

// ============================================================================
// FileSink Implementation
// ============================================================================

FileSink::FileSink(const std::string& filepath, size_t max_size_mb, size_t max_files)
    : filepath_(filepath)
    , max_size_bytes_(max_size_mb * 1024 * 1024)
    , max_files_(max_files)
    , current_size_(0) {
    
    // Create directory if it doesn't exist
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    
    // Open file in append mode
    file_.open(filepath_, std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filepath_);
    }
    
    // Get current file size
    if (std::filesystem::exists(filepath_)) {
        current_size_ = std::filesystem::file_size(filepath_);
    }
}

FileSink::~FileSink() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileSink::write(const LogMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string formatted = msg.format() + "\n";
    
    // Check if rotation is needed before writing
    rotate_if_needed();
    
    file_ << formatted;
    current_size_ += formatted.size();
}

void FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

void FileSink::rotate_if_needed() {
    // Already holding mutex_
    
    if (current_size_ < max_size_bytes_) {
        return;
    }
    
    // Close current file
    file_.close();
    
    // Rotate existing backups
    for (int i = max_files_ - 1; i > 0; --i) {
        std::string old_name = filepath_ + "." + std::to_string(i);
        std::string new_name = filepath_ + "." + std::to_string(i + 1);
        
        if (std::filesystem::exists(old_name)) {
            if (i == static_cast<int>(max_files_ - 1)) {
                // Delete oldest file
                std::filesystem::remove(old_name);
            } else {
                std::filesystem::rename(old_name, new_name);
            }
        }
    }
    
    // Rename current file to .1
    if (std::filesystem::exists(filepath_)) {
        std::string backup = filepath_ + ".1";
        std::filesystem::rename(filepath_, backup);
    }
    
    // Open new file
    file_.open(filepath_, std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to reopen log file after rotation: " + filepath_);
    }
    
    current_size_ = 0;
}

// ============================================================================
// Logger Implementation
// ============================================================================

Logger::Logger(const std::string& name)
    : name_(name), level_(LogLevel::INFO) {}

void Logger::add_sink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(sink);
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level, const std::string& message,
                 const char* file, int line, const char* func) {
    if (!should_log(level)) {
        return;
    }
    
    LogMessage msg;
    msg.level = level;
    msg.timestamp = get_timestamp();
    msg.logger_name = name_;
    msg.message = message;
    msg.file = file ? file : "";
    msg.line = line;
    msg.function = func ? func : "";
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->write(msg);
    }
}

void Logger::trace(const std::string& message, const char* file, int line, const char* func) {
    log(LogLevel::TRACE, message, file, line, func);
}

void Logger::debug(const std::string& message, const char* file, int line, const char* func) {
    log(LogLevel::DEBUG, message, file, line, func);
}

void Logger::info(const std::string& message, const char* file, int line, const char* func) {
    log(LogLevel::INFO, message, file, line, func);
}

void Logger::warn(const std::string& message, const char* file, int line, const char* func) {
    log(LogLevel::WARN, message, file, line, func);
}

void Logger::error(const std::string& message, const char* file, int line, const char* func) {
    log(LogLevel::ERROR, message, file, line, func);
}

void Logger::fatal(const std::string& message, const char* file, int line, const char* func) {
    log(LogLevel::FATAL, message, file, line, func);
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->flush();
    }
}

// ============================================================================
// LoggerRegistry Implementation
// ============================================================================

LoggerRegistry::LoggerRegistry() {}

LoggerRegistry& LoggerRegistry::instance() {
    static LoggerRegistry registry;
    return registry;
}

std::shared_ptr<Logger> LoggerRegistry::get_logger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }
    
    // Create new logger
    auto logger = std::make_shared<Logger>(name);
    loggers_[name] = logger;
    
    return logger;
}

void LoggerRegistry::set_global_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [_, logger] : loggers_) {
        logger->set_level(level);
    }
}

void LoggerRegistry::add_global_sink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [_, logger] : loggers_) {
        logger->add_sink(sink);
    }
}

void LoggerRegistry::flush_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [_, logger] : loggers_) {
        logger->flush();
    }
}

// ============================================================================
// Initialization Function
// ============================================================================

void initialize_logging(LogLevel level, const std::string& log_file) {
    auto& registry = LoggerRegistry::instance();
    
    // Add console sink
    auto console_sink = std::make_shared<ConsoleSink>();
    registry.add_global_sink(console_sink);
    
    // Add file sink if specified
    if (!log_file.empty()) {
        try {
            auto file_sink = std::make_shared<FileSink>(log_file);
            registry.add_global_sink(file_sink);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create file sink: " << e.what() << std::endl;
        }
    }
    
    // Set global log level
    registry.set_global_level(level);
}

} // namespace logging
} // namespace brain_ai
