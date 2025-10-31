#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "cognitive_handler.hpp"
#include "document/document_processor.hpp"
#include "indexing/index_manager.hpp"

namespace py = pybind11;
using namespace brain_ai;

PYBIND11_MODULE(brain_ai_py, m) {
    m.doc() = "Brain-AI C++ cognitive architecture Python bindings";
    
    // FusionWeights
    py::class_<FusionWeights>(m, "FusionWeights")
        .def(py::init<>())
        .def(py::init<float, float, float>(),
             py::arg("vector_weight") = 0.4f,
             py::arg("episodic_weight") = 0.3f,
             py::arg("semantic_weight") = 0.3f)
        .def_readwrite("vector_weight", &FusionWeights::vector_weight)
        .def_readwrite("episodic_weight", &FusionWeights::episodic_weight)
        .def_readwrite("semantic_weight", &FusionWeights::semantic_weight);
    
    // QueryConfig
    py::class_<QueryConfig>(m, "QueryConfig")
        .def(py::init<>())
        .def_readwrite("use_episodic", &QueryConfig::use_episodic)
        .def_readwrite("use_semantic", &QueryConfig::use_semantic)
        .def_readwrite("check_hallucination", &QueryConfig::check_hallucination)
        .def_readwrite("generate_explanation", &QueryConfig::generate_explanation)
        .def_readwrite("top_k_results", &QueryConfig::top_k_results)
        .def_readwrite("hallucination_threshold", &QueryConfig::hallucination_threshold);
    
    // ScoredResult
    py::class_<ScoredResult>(m, "ScoredResult")
        .def(py::init<>())
        .def_readonly("content", &ScoredResult::content)
        .def_readonly("score", &ScoredResult::score)
        .def_readonly("source", &ScoredResult::source);
    
    // QueryResponse
    py::class_<QueryResponse>(m, "QueryResponse")
        .def(py::init<>())
        .def_readonly("query", &QueryResponse::query)
        .def_readonly("response", &QueryResponse::response)
        .def_readonly("results", &QueryResponse::results)
        .def_readonly("overall_confidence", &QueryResponse::overall_confidence)
        .def("to_dict", [](const QueryResponse& r) {
            py::dict d;
            d["query"] = r.query;
            d["response"] = r.response;
            d["confidence"] = r.overall_confidence;
            
            py::list results_list;
            for (const auto& result : r.results) {
                py::dict result_dict;
                result_dict["content"] = result.content;
                result_dict["score"] = result.score;
                result_dict["source"] = result.source;
                results_list.append(result_dict);
            }
            d["results"] = results_list;
            
            return d;
        });
    
    // CognitiveHandler - Main interface
    py::class_<CognitiveHandler>(m, "CognitiveHandler")
        .def(py::init<size_t, FusionWeights, size_t>(),
             py::arg("episodic_capacity") = 128,
             py::arg("fusion_weights") = FusionWeights(),
             py::arg("embedding_dim") = 1536,
             "Initialize cognitive handler with configurable parameters")
        
        .def("process_query", &CognitiveHandler::process_query,
             py::arg("query"),
             py::arg("query_embedding"),
             py::arg("config") = QueryConfig(),
             "Process query through complete cognitive pipeline")
        
        .def("index_document", [](CognitiveHandler& h,
                                  const std::string& doc_id,
                                  const std::vector<float>& embedding,
                                  const std::string& content,
                                  py::dict metadata_dict = py::dict()) {
            // Convert py::dict to nlohmann::json
            nlohmann::json metadata = nlohmann::json::object();
            for (auto item : metadata_dict) {
                std::string key = py::str(item.first);
                py::object value = py::reinterpret_borrow<py::object>(item.second);
                if (py::isinstance<py::str>(value)) {
                    metadata[key] = value.cast<std::string>();
                } else if (py::isinstance<py::int_>(value)) {
                    metadata[key] = value.cast<int>();
                } else if (py::isinstance<py::float_>(value)) {
                    metadata[key] = value.cast<double>();
                } else if (py::isinstance<py::bool_>(value)) {
                    metadata[key] = value.cast<bool>();
                } else {
                    metadata[key] = py::str(value).cast<std::string>();
                }
            }
            return h.index_document(doc_id, embedding, content, metadata);
        }, py::arg("doc_id"),
           py::arg("embedding"),
           py::arg("content"),
           py::arg("metadata") = py::dict(),
           "Index document in vector store with metadata")
        
        .def("batch_index_documents", [](CognitiveHandler& h, py::list docs) {
            std::vector<std::tuple<std::string, std::vector<float>, std::string>> documents;
            for (auto item : docs) {
                auto tuple = item.cast<py::tuple>();
                std::string doc_id = tuple[0].cast<std::string>();
                std::vector<float> embedding = tuple[1].cast<std::vector<float>>();
                std::string content = tuple[2].cast<std::string>();
                documents.push_back(std::make_tuple(doc_id, embedding, content));
            }
            h.batch_index_documents(documents);
        }, py::arg("documents"),
        "Batch index multiple documents efficiently")
        
        .def("add_episode", &CognitiveHandler::add_episode,
             py::arg("query"),
             py::arg("response"),
             py::arg("query_embedding"),
             py::arg("metadata") = std::unordered_map<std::string, std::string>(),
             "Add episode to episodic memory")
        
        .def("save", [](CognitiveHandler& h, const std::string& path) {
            // TODO: Implement full serialization
            // For now, just save vector index
            return h.vector_index().save(path + "/vector_index.bin");
        }, py::arg("path"),
        "Save cognitive handler state to disk")
        
        .def("load", [](CognitiveHandler& h, const std::string& path) {
            // TODO: Implement full deserialization
            // For now, just load vector index
            return h.vector_index().load(path + "/vector_index.bin");
        }, py::arg("path"),
        "Load cognitive handler state from disk")
        
        .def("get_stats", [](const CognitiveHandler& h) {
            py::dict stats;
            stats["episodic_buffer_size"] = h.episodic_buffer_size();
            stats["semantic_network_size"] = h.semantic_network_size();
            stats["vector_index_size"] = h.vector_index_size();
            return stats;
        }, "Get system statistics")
        
        .def("clear_episodic_buffer", [](CognitiveHandler& h) {
            // Access through public interface
            // Note: May need to add clear method to EpisodicBuffer
        }, "Clear episodic buffer")
        
        .def("episodic_buffer_size", &CognitiveHandler::episodic_buffer_size,
             "Get current episodic buffer size")
        .def("semantic_network_size", &CognitiveHandler::semantic_network_size,
             "Get semantic network node count")
        .def("vector_index_size", &CognitiveHandler::vector_index_size,
             "Get vector index document count");
    
    // Version info
    m.attr("__version__") = "4.3.0";
    m.attr("__author__") = "Brain-AI Team";
}

