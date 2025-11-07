# C-AI-BRAIN 🧠

[![Security Scanning](https://github.com/dawsonblock/C-AI-BRAIN/workflows/Security%20Scanning/badge.svg)](https://github.com/dawsonblock/C-AI-BRAIN/actions)
[![CI Tests](https://github.com/dawsonblock/C-AI-BRAIN/workflows/CI%20Tests/badge.svg)](https://github.com/dawsonblock/C-AI-BRAIN/actions)
[![Docker Build](https://github.com/dawsonblock/C-AI-BRAIN/workflows/Docker%20Build%20and%20Publish/badge.svg)](https://github.com/dawsonblock/C-AI-BRAIN/actions)

Production-ready AI microservices architecture for document processing, semantic search, embeddings, and secure computation. Built with security, scalability, and observability as core principles.

## 🚀 Features

### Brain AI REST Service
- **Document Indexing**: Vector embeddings with semantic search
- **Safe Computation**: Mathematical expression evaluation without code execution
- **API Security**: API key authentication, rate limiting, CORS controls
- **Observability**: Prometheus metrics, structured JSON logging, distributed tracing ready
- **Production Hardening**: SQLite WAL mode, connection pooling, health checks

### DeepSeek OCR Service
- **Multi-format Support**: Images (JPEG, PNG, GIF, BMP, TIFF, WebP) and PDFs
- **Security Controls**: MIME validation, file size limits, SHA256 hashing
- **Multiple Modes**: Text, full, document, and layout extraction

## 🏗️ Architecture

```
┌─────────────┐      ┌──────────────────┐      ┌─────────────────┐
│   Clients   │─────▶│   API Gateway    │─────▶│  Brain AI REST  │
│  (Web/API)  │      │  (Proxy + TLS)   │      │    Service      │
└─────────────┘      └──────────────────┘      │   (Port 8000)   │
                              │                 └─────────────────┘
                              │                          │
                              │                          ▼
                              │                  ┌──────────────┐
                              │                  │  SQLite DB   │
                              │                  │  (WAL mode)  │
                              │                  └──────────────┘
                              │
                              └────────────────▶ ┌─────────────────┐
                                                  │  DeepSeek OCR   │
                                                  │    Service      │
                                                  │   (Port 8001)   │
                                                  └─────────────────┘
```

See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for detailed documentation.

## 🔒 Security Features

- ✅ **No Code Execution**: AST-based safe evaluation (no `eval()` or `exec()`)
- ✅ **Authentication**: API key required for all sensitive endpoints
- ✅ **Input Validation**: Pydantic models with size limits
- ✅ **Rate Limiting**: Per-IP rate limits with configurable thresholds
- ✅ **CORS Control**: Disabled by default, explicit origin allowlist
- ✅ **Container Hardening**: Non-root users, read-only filesystem, dropped capabilities
- ✅ **Supply Chain Security**: SHA-pinned actions, automated scanning (Bandit, Trivy, CodeQL)
- ✅ **Security Headers**: CSP, X-Frame-Options, HSTS support
- ✅ **File Validation**: MIME type checks, SHA256 hashing

See [SECURITY.md](SECURITY.md) for security policy and best practices.

## 📋 Prerequisites

- Docker 20.10+
- Docker Compose 2.0+
- (Optional) Python 3.11+ for local development

## 🚀 Quick Start

### 1. Clone Repository

```bash
git clone https://github.com/dawsonblock/C-AI-BRAIN.git
cd C-AI-BRAIN
```

### 2. Configure Environment

```bash
# Copy environment template
cp .env.example .env

# Generate secure API key
openssl rand -hex 32

# Edit .env with your settings
nano .env
```

**Required Configuration**:
```env
API_KEY=your-generated-key-here
CORS_ORIGINS=  # Leave empty for production
ENVIRONMENT=production
```

### 3. Start Services

```bash
# Build and start all services
docker compose up -d

# View logs
docker compose logs -f

# Check health
curl http://localhost:8000/health
curl http://localhost:8001/health
```

### 4. Test API

```bash
# Health check (no auth required)
curl http://localhost:8000/health

# Calculate (requires API key)
curl -X POST http://localhost:8000/calculate \
  -H "X-API-Key: your-api-key" \
  -H "Content-Type: application/json" \
  -d '{"expression": "sqrt(16) * 2"}'

# Index document
curl -X POST http://localhost:8000/index \
  -H "X-API-Key: your-api-key" \
  -H "Content-Type: application/json" \
  -d '{
    "document_id": "doc1",
    "text": "This is a test document for indexing.",
    "metadata": {"source": "test"}
  }'

# OCR processing
curl -X POST http://localhost:8001/ocr \
  -F "file=@image.png" \
  -F "mode=text"
```

## 📊 Monitoring

### Metrics

Prometheus metrics available at `/metrics`:

```bash
curl http://localhost:8000/metrics
```

**Key Metrics**:
- `brain_ai_requests_total` - Request counter by endpoint and status
- `brain_ai_request_duration_seconds` - Request latency histogram
- `brain_ai_documents_indexed_total` - Documents indexed
- `brain_ai_queries_processed_total` - Queries processed
- `brain_ai_calculations_total` - Safe calculations performed

### Logs

Structured JSON logs:

```bash
# View logs
docker compose logs -f brain-ai-rest

# Filter by service
docker compose logs -f deepseek-ocr
```

### Health Checks

```bash
# Liveness (basic health)
curl http://localhost:8000/health

# Readiness (dependency checks)
curl http://localhost:8000/ready
```

## 🧪 Testing

### Run Unit Tests

```bash
# REST service tests
cd brain-ai-rest-service
pip install -r requirements.txt pytest pytest-asyncio pytest-cov
pytest tests/ -v --cov

# OCR service tests
cd deepseek-ocr-service
pip install -r requirements.txt pytest
pytest tests/ -v
```

### Run Integration Tests

```bash
# Start services first
docker compose up -d

# Run integration tests
cd tests/integration
pip install pytest requests
pytest test_integration.py -v

# Cleanup
docker compose down -v
```

## 🔧 Configuration

### Environment Variables

#### Brain AI REST Service

| Variable | Default | Description |
|----------|---------|-------------|
| `API_KEY` | (required) | API authentication key |
| `CORS_ORIGINS` | empty | Comma-separated allowed origins |
| `DB_PATH` | `/data/brain_ai.db` | SQLite database path |
| `EMBEDDING_BACKEND` | `cpu` | Embedding backend (`cpu` or external) |
| `EMBEDDING_MODEL` | `all-MiniLM-L6-v2` | Model for embeddings |
| `LOG_LEVEL` | `INFO` | Logging level |
| `ENVIRONMENT` | `production` | Environment name |
| `METRICS_ENABLED` | `true` | Enable metrics endpoint |
| `RATE_LIMIT_ENABLED` | `true` | Enable rate limiting |
| `RATE_LIMIT_DEFAULT` | `100/minute` | Default rate limit |

#### DeepSeek OCR Service

| Variable | Default | Description |
|----------|---------|-------------|
| `LOG_LEVEL` | `INFO` | Logging level |
| `TMPDIR` | `/tmp/ocr` | Temporary file directory |

### Docker Compose Overrides

Create `docker-compose.override.yml` for local customization:

```yaml
services:
  brain-ai-rest:
    environment:
      - LOG_LEVEL=DEBUG
    ports:
      - "8000:8000"
```

## 🚢 Deployment

### Production Checklist

- [ ] Generate strong API key (`openssl rand -hex 32`)
- [ ] Configure `.env` with production settings
- [ ] Set `ENVIRONMENT=production`
- [ ] Leave `CORS_ORIGINS` empty unless required
- [ ] Deploy behind TLS-terminating reverse proxy
- [ ] Configure persistent volumes for database
- [ ] Set up log aggregation (ELK, Splunk, CloudWatch)
- [ ] Configure Prometheus scraping
- [ ] Set up backup schedule for SQLite
- [ ] Configure monitoring and alerting
- [ ] Review and apply resource limits
- [ ] Test disaster recovery procedures

### Reverse Proxy (nginx example)

```nginx
upstream brain-ai-rest {
    server localhost:8000;
}

server {
    listen 443 ssl http2;
    server_name api.example.com;
    
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    client_max_body_size 2M;
    
    location / {
        proxy_pass http://brain-ai-rest;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

### Kubernetes (Coming Soon)

Helm charts for Kubernetes deployment are planned for future releases.

## 📚 Documentation

- [Architecture](docs/ARCHITECTURE.md) - System architecture and design
- [Security Policy](SECURITY.md) - Security controls and reporting
- [Third-Party Licenses](THIRD_PARTY_LICENSES.md) - Open source attributions
- [API Documentation](http://localhost:8000/docs) - Interactive API docs (dev mode)

## 🛠️ Development

### Local Development Setup

```bash
# Clone repository
git clone https://github.com/dawsonblock/C-AI-BRAIN.git
cd C-AI-BRAIN

# Set up Python environment
python -m venv venv
source venv/bin/activate  # or `venv\Scripts\activate` on Windows

# Install dependencies
cd brain-ai-rest-service
pip install -r requirements.txt
pip install pytest pytest-asyncio pytest-cov black isort flake8

# Run locally
uvicorn app.app:app --reload --port 8000

# Run tests
pytest tests/ -v --cov
```

### Code Quality

```bash
# Format code
black brain-ai-rest-service/app deepseek-ocr-service

# Sort imports
isort brain-ai-rest-service/app deepseek-ocr-service

# Lint
flake8 brain-ai-rest-service/app deepseek-ocr-service --max-line-length=100

# Security scan
bandit -r brain-ai-rest-service/app deepseek-ocr-service
```

### Pre-commit Hooks (Recommended)

```bash
pip install pre-commit
pre-commit install
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Contribution Guidelines

- Write tests for new features
- Maintain 80%+ code coverage
- Follow existing code style (black, isort)
- Update documentation as needed
- Add security considerations to PR description

## 📝 License

This project is proprietary. All rights reserved.

See [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for third-party software licenses.

## 🔐 Security

To report security vulnerabilities, please email [security contact] instead of using the issue tracker.

See [SECURITY.md](SECURITY.md) for our security policy.

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/dawsonblock/C-AI-BRAIN/issues)
- **Discussions**: [GitHub Discussions](https://github.com/dawsonblock/C-AI-BRAIN/discussions)
- **Email**: [support contact]

## 🗺️ Roadmap

- [ ] Multi-tenancy with per-tenant API keys
- [ ] Advanced vector database integration (Pinecone, Weaviate)
- [ ] Full DeepSeek OCR model integration
- [ ] Async job processing with Celery
- [ ] GraphQL API option
- [ ] Kubernetes Helm charts
- [ ] Enhanced monitoring dashboards
- [ ] API rate limiting per tenant
- [ ] Webhook support for async operations

## 🙏 Acknowledgments

- FastAPI for the excellent web framework
- The Python community for amazing libraries
- Security researchers for responsible disclosure

---

**Made with ❤️ by the C-AI-BRAIN Team**

**Last Updated**: November 7, 2025  
**Version**: 1.0.0
