# DeepSeek-OCR Integration Architecture

**Project**: Brain-AI v4.1.0 - Document Preprocessing Pipeline  
**Component**: DeepSeek-OCR Integration Layer  
**Purpose**: High-fidelity document ingestion with OCR → validation → compression → memory storage  
**Status**: Design Phase  
**Date**: 2025-10-31

---

## 🎯 Executive Summary

Integrate DeepSeek-OCR as a document preprocessing layer to create high-fidelity episodic memories from scanned documents, PDFs, and images. This creates a robust **OCR → Validation → Compression → Memory Storage** pipeline that enhances Brain-AI's document understanding capabilities.

### Value Proposition
- **High-Fidelity OCR**: State-of-the-art visual-text compression for documents
- **Structured Output**: Markdown conversion, figure parsing, reference localization
- **Memory Creation**: Episodic memories with rich context from visual documents
- **Multi-Modal**: Seamless integration with existing vector search and cognitive pipeline

---

## 📊 DeepSeek-OCR Analysis

### Core Capabilities
1. **LLM-Centric OCR**: Converts images/documents to structured text
2. **Multiple Resolution Modes**:
   - Tiny (512×512, 64 vision tokens)
   - Small (640×640, 100 tokens)
   - Base (1024×1024, 256 tokens)
   - Large (1280×1280, 400 tokens)
   - Gundam (dynamic: n640×640 + 1024×1024)

3. **Task Support**:
   - Plain OCR extraction
   - Document → Markdown conversion
   - Figure parsing
   - Reference localization
   - Image description

### Technical Stack
- **Framework**: PyTorch 2.6.0 + CUDA 11.8
- **Inference**: vLLM 0.8.5 (batching, streaming) OR Transformers (HF)
- **Acceleration**: Flash Attention 2 (flash-attn==2.7.3)
- **Precision**: bfloat16 (GPU required)
- **Performance**: ~2500 tokens/sec on A100-40G

### Input/Output Formats
**Inputs**:
- Images: PNG/JPEG (PIL.Image, RGB)
- PDFs: Multi-page rasterization
- Prompts: Template-based with grounding tokens

**Outputs**:
- Plain text OCR
- Markdown structured documents
- JSON metadata (with parsing)

---

## 🏗️ Integration Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Client Layer                            │
├─────────────────────────────────────────────────────────────┤
│  gRPC API  │  REST API  │  File Upload  │  Batch Processor │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              Document Ingestion Pipeline (NEW)               │
├─────────────────────────────────────────────────────────────┤
│  1. Document Receiver                                        │
│  2. Format Detector (PDF, Image, Text)                      │
│  3. DeepSeek-OCR Processor (GPU)                            │
│  4. Text Validation & Cleanup                               │
│  5. Embedding Generator                                      │
│  6. Memory Creator                                           │
└─────────────────────────────────────────────────────────────┘
                            │
                ┌───────────┼───────────┐
                ▼           ▼           ▼
┌────────────────┐  ┌──────────────┐  ┌─────────────────┐
│ Vector Index   │  │   Episodic   │  │    Semantic     │
│  (HNSWlib)     │  │    Buffer    │  │     Network     │
│    NEW         │  │   EXISTING   │  │    EXISTING     │
└────────────────┘  └──────────────┘  └─────────────────┘
```

### Component Design

#### 1. DocumentProcessor (C++ Interface)
```cpp
class DocumentProcessor {
public:
    // Process document through OCR pipeline
    ProcessedDocument process(
        const std::string& file_path,
        const DocumentConfig& config = {}
    );
    
    // Batch processing
    std::vector<ProcessedDocument> process_batch(
        const std::vector<std::string>& file_paths,
        ProgressCallback callback = nullptr
    );
    
    // Check if document needs OCR
    bool requires_ocr(const std::string& file_path);
    
private:
    std::unique_ptr<OCRClient> ocr_client_;
    std::unique_ptr<TextValidator> validator_;
    std::unique_ptr<EmbeddingGenerator> embedder_;
};
```

#### 2. OCRClient (Python Service Bridge)
```cpp
class OCRClient {
public:
    // Initialize connection to DeepSeek-OCR service
    explicit OCRClient(const std::string& service_url);
    
    // Submit OCR request
    OCRResult extract_text(
        const std::vector<uint8_t>& image_data,
        const OCRConfig& config
    );
    
    // Health check
    bool is_available();
    
private:
    std::string service_url_;
    std::unique_ptr<HttpClient> http_client_;
};
```

#### 3. DeepSeek-OCR Python Service
```python
# FastAPI service wrapper
from fastapi import FastAPI, UploadFile
from vllm import LLM, SamplingParams
from PIL import Image

app = FastAPI()
llm = LLM(
    model="deepseek-ai/DeepSeek-OCR",
    enable_prefix_caching=False,
    mm_processor_cache_gb=0
)

@app.post("/ocr/extract")
async def extract_text(
    file: UploadFile,
    mode: str = "base",  # tiny, small, base, large, gundam
    task: str = "markdown"  # ocr, markdown, figure, reference
):
    image = Image.open(file.file).convert("RGB")
    
    prompt = get_prompt_template(task)
    model_input = [{
        "prompt": f"<image>\n{prompt}",
        "multi_modal_data": {"image": image}
    }]
    
    sampling_params = SamplingParams(
        temperature=0.0,
        max_tokens=8192
    )
    
    outputs = llm.generate(model_input, sampling_params)
    return {"text": outputs[0].outputs[0].text}

@app.post("/ocr/batch")
async def batch_extract(files: List[UploadFile]):
    # Batch processing for multiple documents
    ...
```

---

## 🔧 Implementation Plan

### Phase 1: Python Service Setup (Day 1)

#### Deliverables
1. **DeepSeek-OCR Service** (`deepseek-ocr-service/`)
   - FastAPI server wrapper
   - vLLM integration
   - Health check endpoint
   - Batch processing support
   - Dockerfile for GPU deployment

2. **Configuration** (`deepseek-ocr-service/config.yaml`)
   ```yaml
   model:
     name: "deepseek-ai/DeepSeek-OCR"
     resolution: "base"  # tiny, small, base, large, gundam
     max_tokens: 8192
     temperature: 0.0
   
   server:
     host: "0.0.0.0"
     port: 8000
     workers: 1  # GPU limited
     timeout: 60
   
   gpu:
     device: "cuda:0"
     dtype: "bfloat16"
     flash_attention: true
   ```

3. **Docker Setup**
   ```dockerfile
   FROM nvidia/cuda:11.8.0-cudnn8-runtime-ubuntu22.04
   
   # Install Python 3.12
   RUN apt-get update && apt-get install -y python3.12 python3-pip
   
   # Install PyTorch
   RUN pip3 install torch==2.6.0 torchvision torchaudio \
       --index-url https://download.pytorch.org/whl/cu118
   
   # Install vLLM and dependencies
   RUN pip3 install vllm==0.8.5 flash-attn==2.7.3 \
       fastapi uvicorn pillow pydantic
   
   # Install DeepSeek-OCR
   RUN pip3 install transformers accelerate
   
   WORKDIR /app
   COPY service.py config.yaml ./
   
   EXPOSE 8000
   CMD ["uvicorn", "service:app", "--host", "0.0.0.0", "--port", "8000"]
   ```

### Phase 2: C++ Integration Layer (Day 2)

#### Deliverables
1. **OCR Client** (`include/document/ocr_client.hpp`)
   ```cpp
   namespace brain_ai {
   namespace document {
   
   struct OCRConfig {
       std::string resolution = "base";
       std::string task = "markdown";
       int max_tokens = 8192;
       float temperature = 0.0f;
   };
   
   struct OCRResult {
       std::string text;
       nlohmann::json metadata;
       float confidence;
       double processing_time_ms;
       bool success;
       std::string error_message;
   };
   
   class OCRClient {
   public:
       explicit OCRClient(const std::string& service_url);
       
       OCRResult extract_text(
           const std::string& file_path,
           const OCRConfig& config = {}
       );
       
       OCRResult extract_text_from_bytes(
           const std::vector<uint8_t>& image_data,
           const OCRConfig& config = {}
       );
       
       bool health_check();
       
   private:
       std::string service_url_;
       std::unique_ptr<httplib::Client> http_client_;
   };
   
   } // namespace document
   } // namespace brain_ai
   ```

2. **Document Processor** (`include/document/document_processor.hpp`)
   ```cpp
   struct ProcessedDocument {
       std::string doc_id;
       std::string original_path;
       std::string extracted_text;
       std::vector<float> embedding;
       nlohmann::json metadata;
       double processing_time_ms;
   };
   
   class DocumentProcessor {
   public:
       DocumentProcessor(
           std::shared_ptr<OCRClient> ocr_client,
           std::shared_ptr<CognitiveHandler> handler
       );
       
       ProcessedDocument process_document(
           const std::string& file_path,
           const DocumentConfig& config = {}
       );
       
       std::vector<ProcessedDocument> process_batch(
           const std::vector<std::string>& file_paths,
           ProgressCallback callback = nullptr
       );
       
       void index_processed_document(
           const ProcessedDocument& doc
       );
       
   private:
       std::shared_ptr<OCRClient> ocr_client_;
       std::shared_ptr<CognitiveHandler> handler_;
       std::unique_ptr<TextValidator> validator_;
   };
   ```

3. **HTTP Client Integration** (cpp-httplib)
   ```cmake
   # Add to CMakeLists.txt
   FetchContent_Declare(
       cpp-httplib
       GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
       GIT_TAG        v0.14.3
   )
   FetchContent_MakeAvailable(cpp-httplib)
   
   target_link_libraries(brain_ai_lib PRIVATE httplib::httplib)
   ```

### Phase 3: Pipeline Integration (Day 3)

#### Deliverables
1. **End-to-End Pipeline**
   ```cpp
   // Example usage
   auto ocr_client = std::make_shared<OCRClient>("http://localhost:8000");
   auto handler = std::make_shared<CognitiveHandler>();
   
   DocumentProcessor processor(ocr_client, handler);
   
   // Process single document
   auto result = processor.process_document(
       "/path/to/document.pdf",
       DocumentConfig{
           .resolution = "base",
           .task = "markdown",
           .create_memory = true,
           .auto_index = true
       }
   );
   
   // Batch processing
   std::vector<std::string> files = {
       "doc1.pdf", "doc2.png", "doc3.jpg"
   };
   
   auto results = processor.process_batch(files, [](size_t processed, size_t total) {
       std::cout << "Progress: " << processed << "/" << total << "\n";
   });
   ```

2. **Memory Creation Integration**
   ```cpp
   void DocumentProcessor::index_processed_document(
       const ProcessedDocument& doc
   ) {
       // 1. Create episodic memory
       handler_->add_episode(
           "Document: " + doc.doc_id,
           doc.extracted_text,
           doc.embedding,
           {
               {"source", "deepseek-ocr"},
               {"original_path", doc.original_path},
               {"processing_time", std::to_string(doc.processing_time_ms)}
           }
       );
       
       // 2. Index in vector search
       handler_->index_document(
           doc.doc_id,
           doc.embedding,
           doc.extracted_text,
           doc.metadata
       );
       
       // 3. Extract and add semantic concepts
       auto concepts = extract_concepts(doc.extracted_text);
       for (const auto& concept : concepts) {
           handler_->semantic_network().add_node(concept);
       }
   }
   ```

3. **Text Validation & Cleanup**
   ```cpp
   class TextValidator {
   public:
       ValidatedText validate(const std::string& raw_ocr_text) {
           ValidatedText result;
           
           // 1. Remove artifacts
           result.text = remove_ocr_artifacts(raw_ocr_text);
           
           // 2. Fix common OCR errors
           result.text = fix_common_errors(result.text);
           
           // 3. Calculate confidence
           result.confidence = calculate_confidence(result.text);
           
           // 4. Extract metadata
           result.metadata = extract_metadata(result.text);
           
           return result;
       }
   };
   ```

---

## 🧪 Testing Strategy

### Unit Tests
1. **OCR Client Tests** (`test_ocr_client.cpp`)
   - Connection to service
   - Error handling
   - Response parsing
   - Timeout handling

2. **Document Processor Tests** (`test_document_processor.cpp`)
   - Single document processing
   - Batch processing
   - Progress callbacks
   - Memory creation

3. **Integration Tests** (`test_ocr_integration.cpp`)
   - End-to-end pipeline
   - Multiple document formats
   - Error recovery
   - Performance benchmarks

### Test Data
```
tests/data/documents/
├── sample_document.pdf
├── scanned_page.png
├── invoice.jpg
├── receipt.png
└── multi_page.pdf
```

---

## 📊 Performance Characteristics

### Expected Latency
- **Single Page (1024×1024)**: ~500ms - 1s
- **Multi-Page PDF (10 pages)**: ~5-10s
- **Batch Processing**: ~2500 tokens/sec throughput

### Resource Requirements
- **GPU**: NVIDIA A100 (40GB) or equivalent
  - Minimum: RTX 3090 (24GB)
  - Cloud: AWS p3.2xlarge, GCP A100
- **Memory**: 40GB GPU RAM for base model
- **CPU**: 8+ cores for preprocessing
- **Storage**: 20GB for model weights

### Scaling Considerations
- **Horizontal**: Multiple GPU instances behind load balancer
- **Vertical**: Batch processing on single GPU
- **Queue**: Redis/RabbitMQ for async processing
- **Cache**: Cache embeddings for repeated documents

---

## 🔒 Security & Compliance

### Data Privacy
1. **Document Retention**: Configure retention policies
2. **PII Handling**: Automatic redaction of sensitive data
3. **Encryption**: TLS for service communication
4. **Access Control**: API keys for OCR service

### Monitoring
1. **Metrics**:
   - OCR requests/sec
   - Processing latency (P50, P95, P99)
   - Error rates
   - GPU utilization

2. **Alerts**:
   - Service unavailable
   - High latency (>5s)
   - GPU out of memory
   - Queue backlog

3. **Logging**:
   - Request/response logs
   - Error traces
   - Performance metrics

---

## 💰 Cost Analysis

### Infrastructure Costs (Monthly)
- **GPU Instance** (AWS p3.2xlarge):
  - On-Demand: ~$3.06/hour × 730 hours = $2,234/month
  - Reserved: ~$1,400/month (1-year commitment)
  - Spot: ~$900-1,200/month (variable)

- **Storage** (S3):
  - Document storage: $23/TB/month
  - Model weights: ~$0.50/month (20GB)

- **Networking**:
  - Data transfer: $0.09/GB
  - Load balancer: $16/month

**Total Estimated Cost**: $1,500-2,500/month (depending on instance type)

### Cost Optimization
1. **Spot Instances**: 60-70% cost reduction
2. **Batch Processing**: Maximize GPU utilization
3. **Auto-scaling**: Scale down during off-hours
4. **Edge Caching**: Reduce redundant OCR requests

---

## 🚀 Deployment Strategy

### Phase 1: Development (Local)
```bash
# 1. Clone DeepSeek-OCR
git clone https://github.com/deepseek-ai/DeepSeek-OCR.git

# 2. Build Docker image
cd deepseek-ocr-service
docker build -t brain-ai-ocr:latest .

# 3. Run service
docker run --gpus all -p 8000:8000 brain-ai-ocr:latest

# 4. Test endpoint
curl -X POST http://localhost:8000/ocr/extract \
  -F "file=@test.pdf" \
  -F "mode=base" \
  -F "task=markdown"
```

### Phase 2: Staging (Cloud)
```bash
# 1. Push to container registry
docker tag brain-ai-ocr:latest gcr.io/project/brain-ai-ocr:v1
docker push gcr.io/project/brain-ai-ocr:v1

# 2. Deploy to GPU instance
gcloud compute instances create ocr-service \
  --machine-type=n1-standard-8 \
  --accelerator=type=nvidia-tesla-a100,count=1 \
  --image-family=pytorch-latest-gpu \
  --boot-disk-size=100GB

# 3. Run container
docker run --gpus all -p 8000:8000 gcr.io/project/brain-ai-ocr:v1
```

### Phase 3: Production (Kubernetes)
```yaml
# kubernetes/ocr-service.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: deepseek-ocr-service
spec:
  replicas: 2
  selector:
    matchLabels:
      app: ocr-service
  template:
    metadata:
      labels:
        app: ocr-service
    spec:
      nodeSelector:
        cloud.google.com/gke-accelerator: nvidia-tesla-a100
      containers:
      - name: ocr-service
        image: gcr.io/project/brain-ai-ocr:v1
        resources:
          limits:
            nvidia.com/gpu: 1
            memory: 50Gi
          requests:
            nvidia.com/gpu: 1
            memory: 40Gi
        ports:
        - containerPort: 8000
---
apiVersion: v1
kind: Service
metadata:
  name: ocr-service
spec:
  type: LoadBalancer
  ports:
  - port: 80
    targetPort: 8000
  selector:
    app: ocr-service
```

---

## 📝 Configuration Files

### 1. Document Processing Config
```yaml
# config/document_processing.yaml
ocr:
  service_url: "http://ocr-service:8000"
  timeout: 60
  retry_attempts: 3
  retry_delay_ms: 1000

processing:
  default_resolution: "base"
  default_task: "markdown"
  max_tokens: 8192
  batch_size: 4
  
validation:
  min_confidence: 0.7
  remove_artifacts: true
  fix_common_errors: true

memory:
  auto_create_episodes: true
  auto_index: true
  importance_threshold: 0.5

performance:
  max_concurrent_requests: 4
  queue_size: 100
  cache_embeddings: true
```

### 2. Prometheus Metrics
```yaml
# Metrics to export
- ocr_requests_total
- ocr_request_duration_seconds
- ocr_errors_total
- ocr_batch_size
- gpu_memory_usage_bytes
- queue_depth
```

---

## 🎯 Success Criteria

### Phase 1 (Service Setup)
- [ ] DeepSeek-OCR service running on GPU
- [ ] Health check endpoint responding
- [ ] Sample PDF processed successfully
- [ ] Docker image < 15GB
- [ ] Latency < 1s for single page

### Phase 2 (C++ Integration)
- [ ] OCR client connecting to service
- [ ] Document processor creating memories
- [ ] Embeddings generated and indexed
- [ ] 100% test coverage for new components
- [ ] Error handling for network issues

### Phase 3 (Production)
- [ ] Kubernetes deployment stable
- [ ] Load balancer distributing traffic
- [ ] Metrics exported to Prometheus
- [ ] Alerts configured
- [ ] Documentation complete

---

## 🔄 Migration Path

### For Existing Documents
```cpp
// Batch reprocess existing documents
DocumentProcessor processor(...);

auto existing_docs = load_existing_document_paths();
auto results = processor.process_batch(existing_docs, [](size_t processed, size_t total) {
    std::cout << "Reprocessing: " << processed << "/" << total << "\n";
});

// Update vector index
for (const auto& result : results) {
    processor.index_processed_document(result);
}
```

---

## 📚 References

- **DeepSeek-OCR GitHub**: https://github.com/deepseek-ai/DeepSeek-OCR
- **arXiv Paper**: arXiv:2510.18234
- **vLLM Documentation**: https://docs.vllm.ai/
- **cpp-httplib**: https://github.com/yhirose/cpp-httplib

---

**Next Steps**:
1. Clone DeepSeek-OCR repository
2. Build and test Python service locally
3. Implement C++ OCR client
4. Integrate with document processor
5. Add to gRPC service layer
6. Deploy to staging environment

**ETA**: 3 days for complete integration  
**Priority**: HIGH (completes document ingestion pipeline)
