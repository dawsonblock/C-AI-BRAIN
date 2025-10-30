#ifndef BRAIN_AI_ERRORS_EXCEPTIONS_HPP
#define BRAIN_AI_ERRORS_EXCEPTIONS_HPP

#include <exception>
#include <string>
#include <sstream>

namespace brain_ai {
namespace errors {

// Base exception for all Brain-AI errors
class BrainAIException : public std::exception {
public:
    explicit BrainAIException(const std::string& message,
                              const std::string& component = "")
        : message_(message), component_(component) {
        format_message();
    }
    
    const char* what() const noexcept override {
        return formatted_message_.c_str();
    }
    
    const std::string& message() const { return message_; }
    const std::string& component() const { return component_; }
    
protected:
    void format_message() {
        std::ostringstream oss;
        if (!component_.empty()) {
            oss << "[" << component_ << "] ";
        }
        oss << message_;
        formatted_message_ = oss.str();
    }
    
    std::string message_;
    std::string component_;
    std::string formatted_message_;
};

// Configuration errors
class ConfigurationError : public BrainAIException {
public:
    explicit ConfigurationError(const std::string& message,
                                const std::string& component = "config")
        : BrainAIException(message, component) {}
};

// Invalid input errors
class InvalidInputError : public BrainAIException {
public:
    explicit InvalidInputError(const std::string& message,
                               const std::string& component = "input")
        : BrainAIException(message, component) {}
};

// Resource errors (memory, file, network)
class ResourceError : public BrainAIException {
public:
    explicit ResourceError(const std::string& message,
                           const std::string& component = "resource")
        : BrainAIException(message, component) {}
};

// Component-specific exceptions

// Episodic buffer errors
class EpisodicBufferError : public BrainAIException {
public:
    explicit EpisodicBufferError(const std::string& message)
        : BrainAIException(message, "episodic_buffer") {}
};

class EpisodicBufferFullError : public EpisodicBufferError {
public:
    explicit EpisodicBufferFullError(size_t capacity)
        : EpisodicBufferError("Buffer is full (capacity: " + 
                              std::to_string(capacity) + ")") {}
};

class EpisodicPersistenceError : public EpisodicBufferError {
public:
    explicit EpisodicPersistenceError(const std::string& message)
        : EpisodicBufferError("Persistence error: " + message) {}
};

// Semantic network errors
class SemanticNetworkError : public BrainAIException {
public:
    explicit SemanticNetworkError(const std::string& message)
        : BrainAIException(message, "semantic_network") {}
};

class NodeNotFoundError : public SemanticNetworkError {
public:
    explicit NodeNotFoundError(const std::string& concept)
        : SemanticNetworkError("Node not found: " + concept) {}
};

class InvalidGraphStructureError : public SemanticNetworkError {
public:
    explicit InvalidGraphStructureError(const std::string& message)
        : SemanticNetworkError("Invalid graph structure: " + message) {}
};

// Hallucination detector errors
class HallucinationDetectionError : public BrainAIException {
public:
    explicit HallucinationDetectionError(const std::string& message)
        : BrainAIException(message, "hallucination_detector") {}
};

class InsufficientEvidenceError : public HallucinationDetectionError {
public:
    explicit InsufficientEvidenceError()
        : HallucinationDetectionError("Insufficient evidence for validation") {}
};

// Hybrid fusion errors
class FusionError : public BrainAIException {
public:
    explicit FusionError(const std::string& message)
        : BrainAIException(message, "hybrid_fusion") {}
};

class InvalidFusionWeightsError : public FusionError {
public:
    explicit InvalidFusionWeightsError(const std::string& message)
        : FusionError("Invalid fusion weights: " + message) {}
};

// Explanation engine errors
class ExplanationError : public BrainAIException {
public:
    explicit ExplanationError(const std::string& message)
        : BrainAIException(message, "explanation_engine") {}
};

// Cognitive handler errors
class CognitiveHandlerError : public BrainAIException {
public:
    explicit CognitiveHandlerError(const std::string& message)
        : BrainAIException(message, "cognitive_handler") {}
};

class QueryProcessingError : public CognitiveHandlerError {
public:
    explicit QueryProcessingError(const std::string& message)
        : CognitiveHandlerError("Query processing failed: " + message) {}
};

// Timeout errors
class TimeoutError : public BrainAIException {
public:
    explicit TimeoutError(const std::string& operation, int timeout_ms)
        : BrainAIException("Operation timed out: " + operation + 
                          " (timeout: " + std::to_string(timeout_ms) + "ms)",
                          "timeout") {}
};

// Validation errors
class ValidationError : public BrainAIException {
public:
    explicit ValidationError(const std::string& message,
                             const std::string& field = "")
        : BrainAIException(message, "validation"), field_(field) {}
    
    const std::string& field() const { return field_; }
    
private:
    std::string field_;
};

// Error builder for detailed error messages
class ErrorBuilder {
public:
    ErrorBuilder() = default;
    
    ErrorBuilder& message(const std::string& msg) {
        message_ = msg;
        return *this;
    }
    
    ErrorBuilder& component(const std::string& comp) {
        component_ = comp;
        return *this;
    }
    
    ErrorBuilder& detail(const std::string& key, const std::string& value) {
        details_[key] = value;
        return *this;
    }
    
    BrainAIException build() const {
        std::ostringstream oss;
        oss << message_;
        if (!details_.empty()) {
            oss << " (";
            bool first = true;
            for (const auto& [key, value] : details_) {
                if (!first) oss << ", ";
                oss << key << ": " << value;
                first = false;
            }
            oss << ")";
        }
        return BrainAIException(oss.str(), component_);
    }
    
private:
    std::string message_;
    std::string component_;
    std::unordered_map<std::string, std::string> details_;
};

// Helper macros for throwing exceptions
#define THROW_ERROR(exception_type, message) \
    throw exception_type(message)

#define THROW_IF(condition, exception_type, message) \
    do { \
        if (condition) { \
            throw exception_type(message); \
        } \
    } while(0)

#define THROW_UNLESS(condition, exception_type, message) \
    THROW_IF(!(condition), exception_type, message)

// Error recovery strategies
enum class RecoveryStrategy {
    FAIL_FAST,           // Throw exception immediately
    RETRY,               // Retry operation
    FALLBACK,            // Use fallback mechanism
    DEGRADE_GRACEFULLY,  // Continue with reduced functionality
    IGNORE               // Log and ignore (use with caution)
};

// Error context for structured error handling
struct ErrorContext {
    std::string operation;
    std::string component;
    std::unordered_map<std::string, std::string> context_data;
    RecoveryStrategy strategy;
    int retry_count = 0;
    int max_retries = 3;
};

} // namespace errors
} // namespace brain_ai

#endif // BRAIN_AI_ERRORS_EXCEPTIONS_HPP
