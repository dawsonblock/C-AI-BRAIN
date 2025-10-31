# DeepSeek-OCR Service

FastAPI-based OCR service compatible with Brain-AI C++ document processing pipeline.

## Features

- **OCR Text Extraction**: Extract text from PDFs and images
- **Multiple Modes**: tiny, small, base, large, gundam
- **Multiple Tasks**: ocr, markdown, figure, reference, describe
- **Health Monitoring**: Built-in health check and statistics endpoints
- **Docker Support**: Easy deployment with Docker Compose

## Quick Start

### Using Docker Compose (Recommended)

```bash
# Start service
docker-compose up --build -d

# Check health
curl http://localhost:8000/health

# View logs
docker-compose logs -f

# Stop service
docker-compose down
```

### Local Development

```bash
# Install dependencies
pip install -r requirements.txt

# Run service
python app.py

# Service runs on http://localhost:8000
```

## API Endpoints

### Health Check
```bash
GET /health
Response: {"status": "healthy", "timestamp": "2024-10-31T..."}
```

### Service Status
```bash
GET /status
Response: {
    "status": "ready",
    "version": "1.0.0",
    "model": "deepseek-ocr-base",
    ...
}
```

### OCR Text Extraction
```bash
POST /ocr/extract
Content-Type: multipart/form-data

Parameters:
- file: Document file (required)
- mode: tiny|small|base|large|gundam (default: base)
- task: ocr|markdown|figure|reference|describe (default: ocr)
- max_tokens: integer (default: 8192)
- temperature: float (default: 0.0)

Response: {
    "text": "Extracted text content...",
    "confidence": 0.95,
    "metadata": {...}
}
```

### Statistics
```bash
GET /stats
Response: {
    "total_requests": 100,
    "successful_requests": 98,
    "failed_requests": 2,
    "success_rate": 0.98,
    "average_processing_time_ms": 250
}
```

## Usage Examples

### Using curl

```bash
# Basic OCR extraction
curl -X POST http://localhost:8000/ocr/extract \
  -F "file=@document.pdf" \
  -F "mode=base" \
  -F "task=ocr"

# Markdown extraction
curl -X POST http://localhost:8000/ocr/extract \
  -F "file=@document.pdf" \
  -F "mode=large" \
  -F "task=markdown"

# Figure description
curl -X POST http://localhost:8000/ocr/extract \
  -F "file=@image.png" \
  -F "mode=base" \
  -F "task=describe"
```

### Using Python

```python
import requests

url = "http://localhost:8000/ocr/extract"

with open("document.pdf", "rb") as f:
    files = {"file": f}
    data = {
        "mode": "base",
        "task": "ocr",
        "max_tokens": 8192
    }
    
    response = requests.post(url, files=files, data=data)
    result = response.json()
    
    print(f"Extracted text: {result['text']}")
    print(f"Confidence: {result['confidence']}")
```

### Using Brain-AI C++ Client

The service is designed to work seamlessly with the Brain-AI C++ OCR client:

```cpp
#include "document/ocr_client.hpp"

brain_ai::document::OCRConfig config;
config.service_url = "http://localhost:8000";
config.mode = "base";
config.task = "markdown";

brain_ai::document::OCRClient client(config);
auto result = client.process_file("/path/to/document.pdf");

if (result.success) {
    std::cout << "Text: " << result.text << std::endl;
    std::cout << "Confidence: " << result.confidence << std::endl;
}
```

## Configuration

### Environment Variables

- `LOG_LEVEL`: Logging level (default: info)
- `PORT`: Service port (default: 8000)
- `MAX_FILE_SIZE_MB`: Maximum file size (default: 50)

### OCR Modes

- **tiny**: Fastest, lowest accuracy (~100ms)
- **small**: Fast, good accuracy (~200ms)
- **base**: Balanced speed/accuracy (~300ms)
- **large**: Slower, high accuracy (~500ms)
- **gundam**: Slowest, highest accuracy (~800ms)

### Task Types

- **ocr**: Plain text extraction
- **markdown**: Structured markdown output
- **figure**: Image/figure description
- **reference**: Citation extraction
- **describe**: Document description

## Integration with Brain-AI

This service is part of the Brain-AI document processing pipeline:

```
Document File
    ↓
OCR Service (this) → Extract text
    ↓
Text Validator → Clean & validate
    ↓
Embedding Generator → Create embeddings
    ↓
Episodic Buffer → Create memory
    ↓
Vector Store → Index document
```

## Monitoring

### Health Checks

The service includes built-in health monitoring:

```bash
# Docker health check
docker ps  # Shows "healthy" status

# Manual health check
curl http://localhost:8000/health

# Detailed status
curl http://localhost:8000/status
```

### Logs

```bash
# View logs
docker-compose logs -f ocr-service

# Follow logs
docker logs -f deepseek-ocr-service
```

## Development

### Mock Implementation

This is currently a **mock implementation** that simulates OCR processing.
For production use, replace the mock functions in `app.py` with actual
DeepSeek OCR model integration.

### Adding Real OCR Model

1. Install DeepSeek OCR dependencies
2. Load model in startup
3. Replace `generate_mock_ocr_result()` with actual model inference
4. Update `calculate_mock_confidence()` with real confidence scores

## Performance

### Expected Latency

- Tiny mode: 100-150ms
- Small mode: 200-250ms
- Base mode: 300-400ms
- Large mode: 500-700ms
- Gundam mode: 800-1200ms

### Throughput

- Concurrent requests: Supports multiple workers
- Max file size: 50MB
- Supported formats: PDF, PNG, JPG, TIFF, etc.

## Troubleshooting

### Service won't start

```bash
# Check port availability
lsof -i :8000

# Check logs
docker-compose logs ocr-service

# Rebuild container
docker-compose up --build --force-recreate
```

### OCR accuracy issues

- Try a higher mode (base → large → gundam)
- Ensure good image quality
- Check file size and format
- Review service logs for errors

### Integration tests failing

```bash
# Verify service is running
curl http://localhost:8000/health

# Check connectivity from Brain-AI
cd /home/user/webapp/brain-ai/build
./tests/integration/test_ocr_integration

# Review test output
ctest -R OCRIntegration --verbose
```

## License

Part of the Brain-AI project.

## Support

For issues and questions, refer to the main Brain-AI documentation.
