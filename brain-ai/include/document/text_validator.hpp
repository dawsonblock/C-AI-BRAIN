#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <regex>

namespace brain_ai::document {

/**
 * @brief Result from text validation
 */
struct ValidationResult {
    std::string cleaned_text;           // Cleaned and validated text
    float confidence;                   // Validation confidence [0.0-1.0]
    size_t errors_corrected;            // Number of corrections made
    std::vector<std::string> warnings;  // Warning messages
    bool is_valid;                      // Overall validation status
    
    ValidationResult() : confidence(1.0f), errors_corrected(0), is_valid(true) {}
};

/**
 * @brief Configuration for text validation
 */
struct ValidationConfig {
    bool remove_ocr_artifacts = true;       // Remove common OCR artifacts
    bool fix_spacing = true;                // Fix spacing issues
    bool fix_line_breaks = true;            // Fix line break issues
    bool normalize_unicode = true;          // Normalize Unicode characters
    bool remove_control_chars = true;       // Remove control characters
    float min_confidence_threshold = 0.5f;  // Minimum acceptable confidence
    
    ValidationConfig() = default;
};

/**
 * @brief Text validation and cleaning for OCR output
 * 
 * Processes OCR-extracted text to:
 * - Remove common OCR artifacts (extra spaces, weird characters)
 * - Fix spacing and line break issues
 * - Normalize Unicode characters
 * - Correct common OCR errors (l/I confusion, 0/O confusion, etc.)
 * - Calculate confidence scores
 * - Generate warnings for suspicious content
 * 
 * Thread-safe: All methods are const or use immutable operations.
 * 
 * Example usage:
 * @code
 *   ValidationConfig config;
 *   config.remove_ocr_artifacts = true;
 *   config.fix_spacing = true;
 *   
 *   TextValidator validator(config);
 *   
 *   std::string ocr_text = "Th1s  is   s0me   text\nwith OCR err0rs.";
 *   auto result = validator.validate(ocr_text);
 *   
 *   if (result.is_valid) {
 *       std::cout << "Cleaned: " << result.cleaned_text << std::endl;
 *       std::cout << "Confidence: " << result.confidence << std::endl;
 *   }
 * @endcode
 */
class TextValidator {
public:
    /**
     * @brief Construct validator with configuration
     * @param config Validation configuration
     */
    explicit TextValidator(const ValidationConfig& config = ValidationConfig());
    
    /**
     * @brief Validate and clean OCR text
     * @param text Raw OCR output text
     * @return Validation result with cleaned text
     */
    ValidationResult validate(const std::string& text) const;
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void update_config(const ValidationConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const ValidationConfig& get_config() const { return config_; }

private:
    ValidationConfig config_;
    
    // Common OCR error patterns
    static const std::unordered_set<std::string> ocr_artifacts_;
    static const std::vector<std::pair<std::regex, std::string>> substitution_patterns_;
    
    /**
     * @brief Remove common OCR artifacts
     * @param text Input text
     * @return Cleaned text
     */
    std::string remove_artifacts(const std::string& text) const;
    
    /**
     * @brief Fix spacing issues
     * @param text Input text
     * @return Text with fixed spacing
     */
    std::string fix_spacing(const std::string& text) const;
    
    /**
     * @brief Fix line break issues
     * @param text Input text
     * @return Text with fixed line breaks
     */
    std::string fix_line_breaks(const std::string& text) const;
    
    /**
     * @brief Normalize Unicode characters
     * @param text Input text
     * @return Normalized text
     */
    std::string normalize_unicode(const std::string& text) const;
    
    /**
     * @brief Remove control characters
     * @param text Input text
     * @return Text without control characters
     */
    std::string remove_control_chars(const std::string& text) const;
    
    /**
     * @brief Apply substitution patterns
     * @param text Input text
     * @param errors_corrected Output: number of corrections made
     * @return Corrected text
     */
    std::string apply_substitutions(const std::string& text, size_t& errors_corrected) const;
    
    /**
     * @brief Calculate confidence score
     * @param original Original text
     * @param cleaned Cleaned text
     * @param errors_corrected Number of corrections made
     * @return Confidence score [0.0-1.0]
     */
    float calculate_confidence(const std::string& original,
                              const std::string& cleaned,
                              size_t errors_corrected) const;
    
    /**
     * @brief Generate validation warnings
     * @param text Text to analyze
     * @return List of warning messages
     */
    std::vector<std::string> generate_warnings(const std::string& text) const;
    
    /**
     * @brief Check if text contains suspicious patterns
     * @param text Text to check
     * @return true if suspicious patterns found
     */
    bool has_suspicious_patterns(const std::string& text) const;
    
    /**
     * @brief Count character type distribution
     * @param text Text to analyze
     * @param alpha Output: alphabetic character count
     * @param digit Output: digit character count
     * @param special Output: special character count
     */
    void count_char_types(const std::string& text,
                         size_t& alpha,
                         size_t& digit,
                         size_t& special) const;
};

} // namespace brain_ai::document
