#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "indexing/index_manager.hpp"

namespace py = pybind11;
using brain_ai::indexing::IndexConfig;
using brain_ai::indexing::IndexManager;
using brain_ai::vector_search::SearchResult;

namespace {

constexpr std::size_t kEmbeddingDim = 384;

std::mutex g_mutex;
std::unique_ptr<IndexManager> g_manager;

IndexManager &ensure_manager() {
    std::scoped_lock<std::mutex> lock(g_mutex);
    if (!g_manager) {
        IndexConfig config;
        config.embedding_dim = kEmbeddingDim;
        config.auto_save = false;
        g_manager = std::make_unique<IndexManager>(config);
    }
    return *g_manager;
}

std::vector<float> to_vector(const py::object &obj) {
    std::vector<float> result;
    if (obj.is_none()) {
        return result;
    }
    for (auto item : py::iter(obj)) {
        result.push_back(py::cast<float>(item));
    }
    if (!result.empty() && result.size() != kEmbeddingDim) {
        throw std::invalid_argument("Embedding dimension mismatch: expected "
                                    + std::to_string(kEmbeddingDim) + ", got "
                                    + std::to_string(result.size()));
    }
    return result;
}

std::vector<float> hashed_embedding(const std::string &text) {
    std::vector<float> vec(kEmbeddingDim, 0.0f);
    std::uint64_t state = 1469598103934665603ULL;  // FNV offset basis
    const std::uint64_t prime = 1099511628211ULL;

    for (unsigned char ch : text) {
        state ^= static_cast<std::uint64_t>(ch);
        state *= prime;
        std::size_t index = static_cast<std::size_t>(state % kEmbeddingDim);
        float value = static_cast<float>((state % 2000) / 1000.0 - 1.0);
        vec[index] += value;
    }

    float norm = 0.0f;
    for (float val : vec) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-6f) {
        for (float &val : vec) {
            val /= norm;
        }
    }
    return vec;
}

}  // namespace

void index_document(const std::string &doc_id,
                    const std::string &text,
                    const py::object &embedding_obj = py::none()) {
    auto &manager = ensure_manager();
    std::vector<float> embedding = to_vector(embedding_obj);
    if (embedding.empty()) {
        embedding = hashed_embedding(text);
    }

    if (!manager.add_document(doc_id, embedding, text)) {
        throw std::runtime_error("Failed to index document: " + doc_id);
    }
}

std::vector<std::pair<std::string, float>> search(const std::string &query,
                                                  int top_k,
                                                  const py::object &embedding_obj = py::none()) {
    auto &manager = ensure_manager();
    std::vector<float> embedding = to_vector(embedding_obj);
    if (embedding.empty()) {
        embedding = hashed_embedding(query);
    }

    if (top_k <= 0) {
        top_k = 5;
    }

    auto results = manager.search(embedding, static_cast<std::size_t>(top_k));
    std::vector<std::pair<std::string, float>> payload;
    payload.reserve(results.size());
    for (const SearchResult &res : results) {
        payload.emplace_back(res.doc_id, res.similarity);
    }
    return payload;
}

void save_index(const std::string &path) {
    auto &manager = ensure_manager();
    if (!manager.save(path)) {
        throw std::runtime_error("Failed to save index to " + path);
    }
}

void load_index(const std::string &path) {
    auto &manager = ensure_manager();
    std::scoped_lock<std::mutex> lock(g_mutex);
    manager.load(path);
}

PYBIND11_MODULE(brain_ai_core, m) {
    m.doc() = "Brain-AI vector index bridge";

    m.def("index_document", &index_document,
          py::arg("doc_id"), py::arg("text"), py::arg("embedding") = py::none(),
          "Index a document using text and optional embedding vector");

    m.def("search", &search,
          py::arg("query"), py::arg("top_k") = 5, py::arg("embedding") = py::none(),
          "Search for documents matching the query");

    m.def("save_index", &save_index, py::arg("path"),
          "Persist index state to disk");

    m.def("load_index", &load_index, py::arg("path"),
          "Load index state from disk if present");
}

