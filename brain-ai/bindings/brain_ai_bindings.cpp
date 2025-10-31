/**
 * @file brain_ai_bindings.cpp
 * @brief Python bindings for Brain-AI C++ cognitive architecture
 * 
 * Exposes CognitiveHandler, IndexManager, and core data types to Python
 * via pybind11 for use in brain-ai-rest-service.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include "cognitive_handler.hpp"
#include "episodic_buffer.hpp"
#include "semantic_network.hpp"
#include "indexing/index_manager.hpp"
#include "vector_search/hnsw_index.hpp"

namespace py = pybind11;
using namespace brain_ai;

// Helper function to convert QueryResponse to Python dict
py::dict query_response_to_dict(const QueryResponse& response) {
    py::dict result;
    result["query"] = response.query;
    result["response"] = response.response;
    result["confidence"] = response.confidence;
    
    py::list results_list;
    for (const auto& scored_result : response.results) {
        py::dict res_dict;
        res_dict["content"] = scored_result.content;
        res_dict["score"] = scored_result.score;
        res_dict["source"] = scored_result.source;
        results_list.append(res_dict);
    }
    result["results"] = results_list;
    
    // Explanation
    py::dict explanation_dict;
    explanation_dict["reasoning_steps"] = py::cast(response.explanation.reasoning_steps);
    explanation_dict["source_attribution"] = py::cast(response.explanation.source_attribution);
    explanation_dict["confidence_breakdown"] = py::cast(response.explanation.confidence_breakdown);
    result["explanation"] = explanation_dict;
    
    // Hallucination check
    result["hallucination_detected"] = response.hallucination_detected;
    result["hallucination_confidence"] = response.hallucination_confidence;
    
    return result;
}

PYBIND11_MODULE(brain_ai_py, m) {
    m.doc() = "Brain-AI Cognitive Architecture - Python bindings";

    // ==================== Core Data Structures ====================
    
    // FusionWeights
    py::class_<FusionWeights>(m, "FusionWeights")
        .def(py::init<>())
        .def(py::init<double, double, double>(),
             py::arg("vector") = 0.5,
             py::arg("episodic") = 0.3,
             py::arg("semantic") = 0.2)
        .def_readwrite("vector", &FusionWeights::vector)
        .def_readwrite("episodic", &FusionWeights::episodic)
        .def_readwrite("semantic", &FusionWeights::semantic)
        .def("__repr__", [](const FusionWeights& w) {
            return "<FusionWeights vector=" + std::to_string(w.vector) +
                   " episodic=" + std::to_string(w.episodic) +
                   " semantic=" + std::to_string(w.semantic) + ">";
        });

    // ScoredResult
    py::class_<ScoredResult>(m, "ScoredResult")
        .def(py::init<>())
        .def_readwrite("content", &ScoredResult::content)
        .def_readwrite("score", &ScoredResult::score)
        .def_readwrite("source", &ScoredResult::source)
        .def("__repr__", [](const ScoredResult& r) {
            return "<ScoredResult score=" + std::to_string(r.score) +
                   " source=" + r.source + ">";
        });

    // Explanation
    py::class_<Explanation>(m, "Explanation")
        .def(py::init<>())
        .def_readwrite("reasoning_steps", &Explanation::reasoning_steps)
        .def_readwrite("source_attribution", &Explanation::source_attribution)
        .def_readwrite("confidence_breakdown", &Explanation::confidence_breakdown);

    // QueryResponse
    py::class_<QueryResponse>(m, "QueryResponse")
        .def(py::init<>())
        .def_readwrite("query", &QueryResponse::query)
        .def_readwrite("response", &QueryResponse::response)
        .def_readwrite("confidence", &QueryResponse::confidence)
        .def_readwrite("results", &QueryResponse::results)
        .def_readwrite("explanation", &QueryResponse::explanation)
        .def_readwrite("hallucination_detected", &QueryResponse::hallucination_detected)
        .def_readwrite("hallucination_confidence", &QueryResponse::hallucination_confidence)
        .def("to_dict", &query_response_to_dict);

    // ==================== EpisodicBuffer ====================
    
    py::class_<EpisodicEntry>(m, "EpisodicEntry")
        .def(py::init<>())
        .def_readwrite("content", &EpisodicEntry::content)
        .def_readwrite("embedding", &EpisodicEntry::embedding)
        .def_readwrite("timestamp", &EpisodicEntry::timestamp)
        .def_readwrite("importance", &EpisodicEntry::importance);

    py::class_<EpisodicBuffer>(m, "EpisodicBuffer")
        .def(py::init<size_t, double>(),
             py::arg("capacity") = 100,
             py::arg("decay_rate") = 0.95)
        .def("add", &EpisodicBuffer::add,
             py::arg("content"),
             py::arg("embedding"),
             py::arg("importance") = 1.0,
             "Add an entry to episodic memory")
        .def("retrieve", &EpisodicBuffer::retrieve,
             py::arg("query_embedding"),
             py::arg("top_k") = 5,
             "Retrieve top-k similar entries")
        .def("size", &EpisodicBuffer::size,
             "Get current buffer size")
        .def("clear", &EpisodicBuffer::clear,
             "Clear all entries");

    // ==================== SemanticNetwork ====================
    
    py::class_<SemanticNetwork>(m, "SemanticNetwork")
        .def(py::init<>())
        .def("add_entity", &SemanticNetwork::add_entity,
             py::arg("entity"),
             "Add an entity to the network")
        .def("add_relation", &SemanticNetwork::add_relation,
             py::arg("entity1"),
             py::arg("entity2"),
             py::arg("relation_type"),
             py::arg("weight") = 1.0,
             "Add a relation between entities")
        .def("get_related", &SemanticNetwork::get_related,
             py::arg("entity"),
             py::arg("relation_type") = "",
             py::arg("max_results") = 10,
             "Get related entities")
        .def("entity_exists", &SemanticNetwork::entity_exists,
             py::arg("entity"),
             "Check if entity exists")
        .def("clear", &SemanticNetwork::clear,
             "Clear the network");

    // ==================== Vector Search ====================
    
    py::class_<vector_search::HNSWIndex>(m, "HNSWIndex")
        .def(py::init<size_t, size_t, size_t, size_t>(),
             py::arg("dim"),
             py::arg("max_elements") = 100000,
             py::arg("M") = 16,
             py::arg("ef_construction") = 200)
        .def("add_document", &vector_search::HNSWIndex::add_document,
             py::arg("doc_id"),
             py::arg("embedding"),
             py::arg("content"),
             py::arg("metadata") = nlohmann::json{},
             "Add a document to the index")
        .def("search", &vector_search::HNSWIndex::search,
             py::arg("query_embedding"),
             py::arg("k") = 10,
             "Search for similar documents")
        .def("save", &vector_search::HNSWIndex::save,
             py::arg("filepath"),
             "Save index to file")
        .def("load", &vector_search::HNSWIndex::load,
             py::arg("filepath"),
             "Load index from file")
        .def("size", &vector_search::HNSWIndex::size,
             "Get number of documents")
        .def("clear", &vector_search::HNSWIndex::clear,
             "Clear the index");

    // ==================== IndexManager ====================
    
    py::class_<IndexConfig>(m, "IndexConfig")
        .def(py::init<>())
        .def_readwrite("embedding_dim", &IndexConfig::embedding_dim)
        .def_readwrite("max_elements", &IndexConfig::max_elements)
        .def_readwrite("M", &IndexConfig::M)
        .def_readwrite("ef_construction", &IndexConfig::ef_construction)
        .def_readwrite("index_path", &IndexConfig::index_path)
        .def_readwrite("auto_save_interval_sec", &IndexConfig::auto_save_interval_sec);

    py::class_<BatchResult>(m, "BatchResult")
        .def(py::init<>())
        .def_readwrite("total", &BatchResult::total)
        .def_readwrite("successful", &BatchResult::successful)
        .def_readwrite("failed", &BatchResult::failed)
        .def_readwrite("processing_time_ms", &BatchResult::processing_time_ms)
        .def("__repr__", [](const BatchResult& r) {
            return "<BatchResult total=" + std::to_string(r.total) +
                   " successful=" + std::to_string(r.successful) +
                   " failed=" + std::to_string(r.failed) +
                   " time=" + std::to_string(r.processing_time_ms) + "ms>";
        });

    py::class_<IndexManager>(m, "IndexManager")
        .def(py::init<const IndexConfig&>())
        .def("add_batch", 
             [](IndexManager& self,
                const std::vector<std::string>& doc_ids,
                const std::vector<std::vector<float>>& embeddings,
                const std::vector<std::string>& contents,
                const std::vector<py::dict>& metadatas_py) {
                 
                 // Convert Python dicts to nlohmann::json
                 std::vector<nlohmann::json> metadatas;
                 for (const auto& meta_py : metadatas_py) {
                     nlohmann::json meta;
                     for (auto item : meta_py) {
                         std::string key = py::str(item.first);
                         py::object value = py::reinterpret_borrow<py::object>(item.second);
                         
                         if (py::isinstance<py::str>(value)) {
                             meta[key] = py::cast<std::string>(value);
                         } else if (py::isinstance<py::int_>(value)) {
                             meta[key] = py::cast<int>(value);
                         } else if (py::isinstance<py::float_>(value)) {
                             meta[key] = py::cast<double>(value);
                         } else if (py::isinstance<py::bool_>(value)) {
                             meta[key] = py::cast<bool>(value);
                         } else {
                             // Default to string
                             meta[key] = py::str(value).cast<std::string>();
                         }
                     }
                     metadatas.push_back(meta);
                 }
                 
                 return self.add_batch(doc_ids, embeddings, contents, metadatas);
             },
             py::arg("doc_ids"),
             py::arg("embeddings"),
             py::arg("contents"),
             py::arg("metadatas") = std::vector<py::dict>{},
             "Add a batch of documents to the index")
        .def("search", &IndexManager::search,
             py::arg("query_embedding"),
             py::arg("k") = 10,
             "Search for similar documents")
        .def("get_document", &IndexManager::get_document,
             py::arg("doc_id"),
             "Get document by ID")
        .def("remove_document", &IndexManager::remove_document,
             py::arg("doc_id"),
             "Remove document from index")
        .def("save", &IndexManager::save,
             "Save index to disk")
        .def("load", &IndexManager::load,
             "Load index from disk")
        .def("size", &IndexManager::size,
             "Get number of documents")
        .def("get_stats", &IndexManager::get_stats,
             "Get index statistics");

    // ==================== CognitiveHandler ====================
    
    py::class_<CognitiveHandler>(m, "CognitiveHandler")
        .def(py::init<size_t, const FusionWeights&, size_t>(),
             py::arg("episodic_capacity") = 128,
             py::arg("fusion_weights") = FusionWeights(),
             py::arg("embedding_dim") = 1536,
             "Initialize CognitiveHandler with specified parameters")
        .def("process_query",
             [](CognitiveHandler& self, 
                const std::string& query,
                const std::vector<float>& query_embedding) {
                 auto response = self.process_query(query, query_embedding);
                 return query_response_to_dict(response);
             },
             py::arg("query"),
             py::arg("query_embedding"),
             "Process a query and return cognitive response as dict")
        .def("index_document", &CognitiveHandler::index_document,
             py::arg("doc_id"),
             py::arg("embedding"),
             py::arg("content"),
             py::arg("metadata") = nlohmann::json{},
             "Index a document")
        .def("add_episode", &CognitiveHandler::add_episode,
             py::arg("content"),
             py::arg("embedding"),
             py::arg("importance") = 1.0,
             "Add an episode to memory")
        .def("get_recent_episodes", &CognitiveHandler::get_recent_episodes,
             py::arg("count") = 10,
             "Get recent episodes")
        .def("add_semantic_relation", &CognitiveHandler::add_semantic_relation,
             py::arg("entity1"),
             py::arg("entity2"),
             py::arg("relation_type"),
             py::arg("weight") = 1.0,
             "Add a semantic relation")
        .def("get_stats",
             [](const CognitiveHandler& self) {
                 auto stats = self.get_stats();
                 py::dict result;
                 result["episodic_count"] = stats.episodic_count;
                 result["vector_index_size"] = stats.vector_index_size;
                 result["semantic_entity_count"] = stats.semantic_entity_count;
                 result["semantic_relation_count"] = stats.semantic_relation_count;
                 return result;
             },
             "Get system statistics")
        .def("save_state", &CognitiveHandler::save_state,
             py::arg("base_path"),
             "Save cognitive state to disk")
        .def("load_state", &CognitiveHandler::load_state,
             py::arg("base_path"),
             "Load cognitive state from disk");

    // ==================== Utility Functions ====================
    
    m.def("cosine_similarity",
          [](const std::vector<float>& a, const std::vector<float>& b) {
              if (a.size() != b.size() || a.empty()) {
                  throw std::runtime_error("Vectors must have same non-zero size");
              }
              
              float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
              for (size_t i = 0; i < a.size(); ++i) {
                  dot += a[i] * b[i];
                  norm_a += a[i] * a[i];
                  norm_b += b[i] * b[i];
              }
              
              if (norm_a < 1e-10f || norm_b < 1e-10f) {
                  return 0.0f;
              }
              
              return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
          },
          py::arg("a"),
          py::arg("b"),
          "Compute cosine similarity between two vectors");

    // Version info
    m.attr("__version__") = "4.3.0";
    m.attr("__author__") = "Brain-AI Team";
}
