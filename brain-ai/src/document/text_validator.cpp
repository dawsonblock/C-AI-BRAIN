#include "document/text_validator.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

// Logger placeholder (replace with full logging system if available)
namespace Logger {
    inline void info(const std::string& component, const std::string& message) {
        std::cout << "[INFO] " << component << ": " << message << std::endl;
    }
}

namespace brain_ai::document {

// Common OCR artifacts to remove
const std::unordered_set<std::string> TextValidator::ocr_artifacts_ = {
    "�",        // Replacement character
    "\uFFFD",   // Unicode replacement character
    "|||",      // Vertical bars
    "___",      // Underscores
    "...",      // Excessive dots (when not ellipsis)
};

// Regex patterns for common OCR errors (using simpler patterns without lookbehind/lookahead)
const std::vector<std::pair<std::regex, std::string>> TextValidator::substitution_patterns_ = {
    // Punctuation fixes
    {std::regex(R"(\s+([,.!?;:]))"), "$1"},           // Remove space before punctuation
    {std::regex(R"(([,.!?;:])\s*([,.!?;:]))"), "$1"}, // Remove duplicate punctuation
    
    // Quote fixes
    {std::regex(R"(``)"), "\""},                      // `` -> "
    {std::regex(R"('')"), "\""},                      // '' -> "
    
    // Hyphen fixes
    {std::regex(R"(\s+-\s+)"), " - "},                // Normalize dash spacing
    {std::regex(R"(--+)"), "\xE2\x80\x94"},           // -- -> em dash
    
    // Simple word boundary fixes (without lookahead/lookbehind)
    {std::regex(R"(\brn\b)"), "m"},                   // rn -> m
    {std::regex(R"(\bvv)"), "w"},                     // vv -> w
};

TextValidator::TextValidator(const ValidationConfig& config)
    : config_(config) {
    Logger::info("TextValidator", "Initialized with validation rules");
}

ValidationResult TextValidator::validate(const std::string& text) const {
    ValidationResult result;
    
    if (text.empty()) {
        result.is_valid = false;
        result.confidence = 0.0f;
        result.warnings.push_back("Empty text input");
        return result;
    }
    
    std::string cleaned = text;
    
    // Apply cleaning steps
    if (config_.remove_control_chars) {
        cleaned = remove_control_chars(cleaned);
    }
    
    if (config_.normalize_unicode) {
        cleaned = normalize_unicode(cleaned);
    }
    
    if (config_.remove_ocr_artifacts) {
        cleaned = remove_artifacts(cleaned);
    }
    
    // Fix line breaks BEFORE fix_spacing, so hyphenated words can be detected
    if (config_.fix_line_breaks) {
        cleaned = fix_line_breaks(cleaned);
    }
    
    if (config_.fix_spacing) {
        cleaned = fix_spacing(cleaned);
    }
    
    // Apply substitution patterns
    size_t errors_corrected = 0;
    cleaned = apply_substitutions(cleaned, errors_corrected);
    
    // Calculate confidence
    float confidence = calculate_confidence(text, cleaned, errors_corrected);
    
    // Generate warnings
    auto warnings = generate_warnings(cleaned);
    
    // Populate result
    result.cleaned_text = cleaned;
    result.confidence = confidence;
    result.errors_corrected = errors_corrected;
    result.warnings = warnings;
    result.is_valid = confidence >= config_.min_confidence_threshold;
    
    Logger::info("TextValidator", 
                "Validation complete: confidence=" + std::to_string(confidence) +
                ", errors=" + std::to_string(errors_corrected) +
                ", warnings=" + std::to_string(warnings.size()));
    
    return result;
}

void TextValidator::update_config(const ValidationConfig& config) {
    config_ = config;
    Logger::info("TextValidator", "Configuration updated");
}

std::string TextValidator::remove_artifacts(const std::string& text) const {
    std::string result = text;
    
    // Remove known artifacts
    for (const auto& artifact : ocr_artifacts_) {
        size_t pos = 0;
        while ((pos = result.find(artifact, pos)) != std::string::npos) {
            result.erase(pos, artifact.length());
        }
    }
    
    return result;
}

std::string TextValidator::fix_spacing(const std::string& text) const {
    std::string result;
    result.reserve(text.size());
    
    bool prev_space = false;
    for (char c : text) {
        if (std::isspace(c)) {
            if (!prev_space && !result.empty()) {
                result += ' ';
            }
            prev_space = true;
        } else {
            result += c;
            prev_space = false;
        }
    }
    
    // Trim leading/trailing spaces
    auto start = result.find_first_not_of(' ');
    auto end = result.find_last_not_of(' ');
    
    if (start != std::string::npos && end != std::string::npos) {
        return result.substr(start, end - start + 1);
    }
    
    return result;
}

std::string TextValidator::fix_line_breaks(const std::string& text) const {
    std::string result = text;
    
    // Fix hyphenated line breaks (word-\nword -> wordword, removing hyphen)
    std::regex hyphen_break(R"((\w)-\s*\n\s*(\w))");
    result = std::regex_replace(result, hyphen_break, "$1$2");
    
    // Fix broken words across lines (word\nword -> word word)
    std::regex word_break(R"((\w)\n(\w))");
    result = std::regex_replace(result, word_break, "$1 $2");
    
    // Normalize multiple newlines to paragraph breaks
    std::regex multi_newline(R"(\n\s*\n+)");
    result = std::regex_replace(result, multi_newline, "\n\n");
    
    return result;
}

std::string TextValidator::normalize_unicode(const std::string& text) const {
    std::string result = text;
    
    // Replace common Unicode look-alikes with ASCII equivalents
    static const std::vector<std::pair<std::string, std::string>> replacements = {
        {"\xE2\x80\x99", "'"},      // Right single quote -> apostrophe
        {"\xE2\x80\x98", "'"},      // Left single quote -> apostrophe
        {"\xE2\x80\x9C", "\""},     // Left double quote
        {"\xE2\x80\x9D", "\""},     // Right double quote
        {"\xE2\x80\x93", "-"},      // En dash
        {"\xE2\x80\x94", "-"},      // Em dash
        {"\xE2\x80\xA6", "..."},    // Ellipsis
        {"\xE2\x80\xA2", "*"},      // Bullet
        {"\xC2\xB0", " degrees "},  // Degree symbol
    };
    
    for (const auto& [from, to] : replacements) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    
    return result;
}

std::string TextValidator::remove_control_chars(const std::string& text) const {
    std::string result;
    result.reserve(text.size());
    
    for (unsigned char c : text) {
        // Keep printable chars, newlines, tabs, carriage returns
        if (c >= 32 || c == '\n' || c == '\t' || c == '\r') {
            result += c;
        }
    }
    
    return result;
}

std::string TextValidator::apply_substitutions(const std::string& text,
                                               size_t& errors_corrected) const {
    std::string result = text;
    errors_corrected = 0;
    
    for (const auto& [pattern, replacement] : substitution_patterns_) {
        std::string prev = result;
        result = std::regex_replace(result, pattern, replacement);
        
        // Count changes (approximate)
        if (prev != result) {
            errors_corrected++;
        }
    }
    
    return result;
}

float TextValidator::calculate_confidence(const std::string& original,
                                         const std::string& cleaned,
                                         size_t errors_corrected) const {
    if (original.empty()) return 0.0f;
    
    // Base confidence on edit distance ratio
    float size_ratio = static_cast<float>(cleaned.size()) / original.size();
    
    // Penalize for large size differences
    float size_score = 1.0f - std::abs(1.0f - size_ratio);
    
    // Penalize for corrections
    float correction_penalty = std::min(1.0f, errors_corrected / 10.0f);
    float correction_score = 1.0f - (correction_penalty * 0.3f);
    
    // Check for suspicious patterns
    float pattern_score = has_suspicious_patterns(cleaned) ? 0.7f : 1.0f;
    
    // Character distribution check
    size_t alpha, digit, special;
    count_char_types(cleaned, alpha, digit, special);
    
    float total_chars = alpha + digit + special;
    float alpha_ratio = total_chars > 0 ? alpha / total_chars : 0.0f;
    
    // Expect mostly alphabetic characters
    float dist_score = (alpha_ratio > 0.5f) ? 1.0f : alpha_ratio * 2.0f;
    
    // Combined confidence (weighted average)
    float confidence = (size_score * 0.3f) +
                      (correction_score * 0.3f) +
                      (pattern_score * 0.2f) +
                      (dist_score * 0.2f);
    
    return std::clamp(confidence, 0.0f, 1.0f);
}

std::vector<std::string> TextValidator::generate_warnings(const std::string& text) const {
    std::vector<std::string> warnings;
    
    if (text.empty()) {
        warnings.push_back("Text is empty after cleaning");
        return warnings;
    }
    
    // Check for very short text
    if (text.size() < 10) {
        warnings.push_back("Text is very short (" + std::to_string(text.size()) + " chars)");
    }
    
    // Check character distribution
    size_t alpha, digit, special;
    count_char_types(text, alpha, digit, special);
    
    float total = alpha + digit + special;
    if (total > 0) {
        float special_ratio = special / total;
        if (special_ratio > 0.3f) {
            warnings.push_back("High ratio of special characters (" + 
                             std::to_string(static_cast<int>(special_ratio * 100)) + "%)");
        }
    }
    
    // Check for suspicious patterns
    if (has_suspicious_patterns(text)) {
        warnings.push_back("Text contains suspicious patterns");
    }
    
    // Check for excessive repetition
    std::regex repeat_pattern(R"((.)\1{5,})");
    if (std::regex_search(text, repeat_pattern)) {
        warnings.push_back("Text contains excessive character repetition");
    }
    
    return warnings;
}

bool TextValidator::has_suspicious_patterns(const std::string& text) const {
    // Patterns that might indicate OCR failures
    static const std::vector<std::regex> suspicious_patterns = {
        std::regex(R"([^\x00-\x7F]{10,})"),           // Long non-ASCII sequences
        std::regex(R"(\d{20,})"),                     // Very long number sequences
        std::regex(R"([A-Z]{15,})"),                  // Very long uppercase sequences
        std::regex(R"((.)\1{10,})"),                  // Character repeated 10+ times
        std::regex(R"([\x00-\x1F\x7F]{3,})"),        // Control character sequences
    };
    
    for (const auto& pattern : suspicious_patterns) {
        if (std::regex_search(text, pattern)) {
            return true;
        }
    }
    
    return false;
}

void TextValidator::count_char_types(const std::string& text,
                                     size_t& alpha,
                                     size_t& digit,
                                     size_t& special) const {
    alpha = 0;
    digit = 0;
    special = 0;
    
    for (unsigned char c : text) {
        if (std::isalpha(c)) {
            alpha++;
        } else if (std::isdigit(c)) {
            digit++;
        } else if (!std::isspace(c)) {
            special++;
        }
    }
}

} // namespace brain_ai::document
