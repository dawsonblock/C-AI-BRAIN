#ifndef BRAIN_AI_UTILS_HPP
#define BRAIN_AI_UTILS_HPP

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace brain_ai {

// Cosine similarity between two vectors
inline float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vectors must have same dimension");
    }
    
    if (a.empty()) {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);
    
    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 0.0f;
    }
    
    return dot_product / (norm_a * norm_b);
}

// L2 (Euclidean) distance
inline float l2_distance(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vectors must have same dimension");
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    
    return std::sqrt(sum);
}

// Normalize vector to unit length
inline std::vector<float> normalize_vector(const std::vector<float>& v) {
    float norm = 0.0f;
    for (float val : v) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    
    if (norm == 0.0f) {
        return v;
    }
    
    std::vector<float> result(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        result[i] = v[i] / norm;
    }
    
    return result;
}

// Sigmoid function
inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

// Softmax function
inline std::vector<float> softmax(const std::vector<float>& logits) {
    if (logits.empty()) {
        return {};
    }
    
    // Numerical stability: subtract max
    float max_logit = *std::max_element(logits.begin(), logits.end());
    
    std::vector<float> exp_values(logits.size());
    float sum = 0.0f;
    
    for (size_t i = 0; i < logits.size(); ++i) {
        exp_values[i] = std::exp(logits[i] - max_logit);
        sum += exp_values[i];
    }
    
    for (size_t i = 0; i < exp_values.size(); ++i) {
        exp_values[i] /= sum;
    }
    
    return exp_values;
}

// Weighted average of vectors
inline std::vector<float> weighted_average(
    const std::vector<std::vector<float>>& vectors,
    const std::vector<float>& weights
) {
    if (vectors.empty() || weights.empty()) {
        return {};
    }
    
    if (vectors.size() != weights.size()) {
        throw std::invalid_argument("Vectors and weights must have same count");
    }
    
    size_t dim = vectors[0].size();
    std::vector<float> result(dim, 0.0f);
    
    for (size_t i = 0; i < vectors.size(); ++i) {
        if (vectors[i].size() != dim) {
            throw std::invalid_argument("All vectors must have same dimension");
        }
        
        for (size_t j = 0; j < dim; ++j) {
            result[j] += vectors[i][j] * weights[i];
        }
    }
    
    return result;
}

// Simple string tokenization
inline std::vector<std::string> tokenize(const std::string& text, char delimiter = ' ') {
    std::vector<std::string> tokens;
    std::string token;
    
    for (char c : text) {
        if (c == delimiter) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    
    if (!token.empty()) {
        tokens.push_back(token);
    }
    
    return tokens;
}

// Convert string to lowercase
inline std::string to_lowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Check if string contains substring (case-insensitive)
inline bool contains_case_insensitive(const std::string& haystack, const std::string& needle) {
    std::string hay_lower = to_lowercase(haystack);
    std::string needle_lower = to_lowercase(needle);
    return hay_lower.find(needle_lower) != std::string::npos;
}

} // namespace brain_ai

#endif // BRAIN_AI_UTILS_HPP
