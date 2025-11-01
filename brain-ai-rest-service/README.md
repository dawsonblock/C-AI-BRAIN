# Brain-AI REST Service

HTTP/JSON REST API service for Brain-AI, providing the same functionality as the planned gRPC service but with easier deployment and testing.

## Overview

This service provides a REST API wrapper around the Brain-AI C++ library, making it accessible via HTTP/JSON instead of gRPC. It provides all the same endpoints as defined in `brain_ai.proto` but uses standard REST conventions.

## Why REST Instead of gRPC?

While gRPC was originally planned, a REST API provides:
- **Easier Testing**: Use curl, Postman, or any HTTP client
- **Simpler Deployment**: No protobuf compilation needed
- **Better Compatibility**: Works with any HTTP client
- **Faster Development**: Python FastAPI is quick to implement
- **Same Functionality**: All RPC methods mapped to REST endpoints

## Architecture

```
Client Application
    ↓ HTTP/JSON
REST Service (Python FastAPI)
    ↓ C++ bindings (pybind11)
Brain-AI C++ Library
    ↓
Document Processing Pipeline
```

## API Endpoints

### Document Processing

**POST /api/v1/documents/process**
Process a single document through OCR and indexing pipeline.

Request:
```json
{
  "doc_id": "doc_001",
  "file_path": "/path/to/document.pdf",
  "ocr_config": {
    "service_url": "http://localhost:8000",
    "mode": "base",
    "task": "ocr"
  },
  "create_memory": true,
  "index_document": true
}
```

Response:
```json
{
  "success": true,
  "doc_id": "doc_001",
  "extracted_text": "Document content...",
  "validated_text": "Cleaned content...",
  "ocr_confidence": 0.95,
  "validation_confidence": 0.88,
  "indexed": true,
  "processing_time_ms": 450,
  "error_message": ""
}
```

**POST /api/v1/documents/batch**
Process multiple documents in batch.

Request:
```json
{
  "documents": [
    {"doc_id": "doc_001", "file_path": "/path/to/doc1.pdf"},
    {"doc_id": "doc_002", "file_path": "/path/to/doc2.pdf"}
  ],
  "ocr_config": {...}
}
```

Response:
```json
{
  "total": 2,
  "successful": 2,
  "failed": 0,
  "results": [...]
}
```

### Query Processing

**POST /api/v1/query**
Process a query and retrieve relevant information.

Request:
```json
{
  "query": "What is the main topic?",
  "query_embedding": [0.1, 0.2, ...],  // 1536-dim vector
  "top_k": 10,
  "use_episodic": true,
  "use_semantic": true
}
```

Response:
```json
{
  "query": "What is the main topic?",
  "response": "The main topic is...",
  "confidence": 0.92,
  "results": [
    {
      "content": "Result 1",
      "score": 0.95,
      "source": "vector"
    }
  ],
  "explanation": {
    "reasoning": "Based on vector search...",
    "confidence": 0.90
  },
  "processing_time_ms": 250
}
```

### Vector Search

**POST /api/v1/search**
Search for similar documents using vector similarity.

Request:
```json
{
  "query_embedding": [0.1, 0.2, ...],
  "top_k": 10,
  "similarity_threshold": 0.7
}
```

Response:
```json
{
  "results": [
    {
      "doc_id": "doc_001",
      "content": "Document content...",
      "similarity": 0.95,
      "metadata": {...}
    }
  ],
  "total_results": 5
}
```

**POST /api/v1/index**
Index a document in the vector store.

Request:
```json
{
  "doc_id": "doc_001",
  "embedding": [0.1, 0.2, ...],
  "content": "Document content",
  "metadata": {"source": "pdf", "author": "John"}
}
```

Response:
```json
{
  "success": true,
  "doc_id": "doc_001",
  "indexed": true
}
```

### Episodic Memory

**POST /api/v1/episodes**
Add an episode to episodic memory.

Request:
```json
{
  "query": "What is AI?",
  "response": "AI is artificial intelligence...",
  "query_embedding": [0.1, 0.2, ...],
  "metadata": {"timestamp": "2024-10-31T10:00:00Z"}
}
```

Response:
```json
{
  "success": true,
  "episode_id": "ep_12345"
}
```

**GET /api/v1/episodes/recent?limit=10**
Get recent episodes.

Response:
```json
{
  "episodes": [
    {
      "query": "What is AI?",
      "response": "AI is...",
      "timestamp": "2024-10-31T10:00:00Z",
      "relevance": 1.0
    }
  ],
  "total": 5
}
```

**POST /api/v1/episodes/search**
Search episodes by query.

Request:
```json
{
  "query_embedding": [0.1, 0.2, ...],
  "top_k": 5,
  "min_similarity": 0.7
}
```

### Health and Statistics

**GET /api/v1/health**
Health check endpoint.

Response:
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "timestamp": "2024-10-31T10:00:00Z",
  "components": {
    "ocr_service": "healthy",
    "vector_store": "healthy",
    "episodic_buffer": "healthy"
  }
}
```


### Monitoring & Metrics

**GET /metrics**
Prometheus-formatted scrape point (disabled by default—set `monitoring.prometheus_enabled: true` in `config.yaml`).

Exports include:
- `brain_ai_requests_total{method="POST",result="success"}` – per-method request counter
- `brain_ai_request_latency_seconds` – raw latency samples with p50 / p95 / p99 summaries (keeps the last 1,024 samples per method/result label while tracking full totals)
- `brain_ai_facts_count` – total cached high-confidence facts
- Gauges reflecting vector index, episodic buffer, and semantic graph sizes when the C++ backend is active

Tuning:
- `BRAIN_AI_METRICS_HIST_MAX_SAMPLES` controls the rolling window size for latency histograms (default 1024, capped at 100k, non-positive values ignored).


**GET /api/v1/stats**
Service statistics.

Response:
```json
{
  "total_documents": 150,
  "total_queries": 500,
  "episodic_buffer_size": 128,
  "vector_index_size": 150,
  "semantic_network_nodes": 50,
  "uptime_seconds": 3600,
  "avg_query_time_ms": 250,
  "avg_document_time_ms": 450
}
```

## Quick Start

### Installation

```bash
cd brain-ai-rest-service
pip install -r requirements.txt
```

### Start Service

```bash
python app.py
# Service runs on http://localhost:5001
```

### Test Endpoints

```bash
# Health check
curl http://localhost:5001/api/v1/health

# Process document
curl -X POST http://localhost:5001/api/v1/documents/process \
  -H "Content-Type: application/json" \
  -d '{
    "doc_id": "test_001",
    "file_path": "/tmp/test.pdf",
    "ocr_config": {
      "service_url": "http://localhost:8000",
      "mode": "base"
    }
  }'

# Query
curl -X POST http://localhost:5001/api/v1/query \
  -H "Content-Type: application/json" \
  -d '{
    "query": "test query",
    "query_embedding": [0.1, 0.2, 0.3, ...],
    "top_k": 5
  }'
```

## Integration with Brain-AI C++

The REST service acts as a wrapper around the Brain-AI C++ library. It will:

1. Accept HTTP/JSON requests
2. Call C++ functions via Python bindings (pybind11)
3. Return JSON responses

For now, we'll implement a Python-only version that simulates the C++ backend until pybind11 bindings are created.

## Configuration

Environment variables:
- `REST_SERVICE_PORT`: Service port (default: 5001)
- `OCR_SERVICE_URL`: OCR service URL (default: http://localhost:8000)
- `LOG_LEVEL`: Logging level (default: info)
- `MAX_WORKERS`: Number of worker threads (default: 4)

## Future: Migration to gRPC

Once gRPC tools are available:
1. Keep REST API for compatibility
2. Add gRPC service alongside REST
3. Both services can coexist
4. Clients can choose REST or gRPC

## Performance

Expected latency:
- Health check: < 10ms
- Document processing: 400-800ms (OCR-dependent)
- Query processing: 200-400ms
- Vector search: 50-150ms
- Statistics: < 50ms

## Security

For production:
- Add API key authentication
- Enable HTTPS/TLS
- Rate limiting
- Input validation
- CORS configuration

## License

Part of the Brain-AI project.
