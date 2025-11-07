# C-AI-BRAIN Architecture

## Overview

C-AI-BRAIN is a production-ready microservices architecture for AI-powered document processing, embedding generation, and secure computation. The system consists of multiple independent services designed with security, scalability, and observability as core principles.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Client Layer                         │
│  (Web UI, API Clients, External Integrations)              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     API Gateway / Proxy                      │
│           (Rate Limiting, TLS, Auth, Load Balance)          │
└─────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴─────────┐
                    ▼                   ▼
        ┌───────────────────┐  ┌──────────────────┐
        │  Brain AI REST    │  │  DeepSeek OCR    │
        │     Service       │  │    Service       │
        │   (Port 8000)     │  │   (Port 8001)    │
        └───────────────────┘  └──────────────────┘
                │                       │
                ▼                       ▼
        ┌──────────────┐      ┌──────────────┐
        │   SQLite DB  │      │   Temp Files │
        │   (WAL mode) │      │   (tmpfs)    │
        └──────────────┘      └──────────────┘
```

## Services

### 1. Brain AI REST Service

**Purpose**: Core AI service for document indexing, embeddings, semantic search, and secure mathematical computation.

**Key Features**:
- Document indexing with vector embeddings
- Semantic search over indexed documents
- Safe mathematical expression evaluation (no code execution)
- API key authentication
- Rate limiting (100 req/min default)
- Prometheus metrics
- JSON structured logging

**Technology Stack**:
- Python 3.11
- FastAPI (async web framework)
- SQLite with WAL mode (embeddings & cache)
- Pydantic v2 (validation)
- prometheus-client (metrics)

**Security Controls**:
- AST-based safe evaluator (no `eval()` or `exec()`)
- API key authentication on all endpoints
- CORS locked to explicit origins (default: disabled)
- Request body size limits (1MB default)
- Rate limiting per IP
- Security headers (CSP, X-Frame-Options, etc.)
- Non-root container user (UID 1000)
- Read-only filesystem with writable `/data` volume

**Endpoints**:
- `GET /health`, `/healthz` - Health check (no auth)
- `GET /ready`, `/readyz` - Readiness check with dependency validation
- `GET /metrics` - Prometheus metrics (if enabled)
- `POST /index` - Index document with embeddings (requires auth)
- `POST /query` - Semantic search (requires auth)
- `POST /calculate` - Safe math evaluation (requires auth)

**Configuration**:
- Environment variables (see `.env.example`)
- API key required in production
- CORS origins must be explicitly set
- SQLite path, embedding backend, log level configurable

### 2. DeepSeek OCR Service

**Purpose**: Optical Character Recognition service for images and PDFs with security hardening.

**Key Features**:
- OCR for images (JPEG, PNG, GIF, BMP, TIFF, WebP)
- PDF document processing (up to 100 pages)
- Multiple modes: text, full, document, layout
- File hash tracking (SHA256)
- MIME type validation

**Technology Stack**:
- Python 3.11
- FastAPI
- File type validation

**Security Controls**:
- File size limit (50MB)
- MIME type allowlist
- SHA256 hashing for traceability
- PDF page count limit (100 pages)
- Temporary file cleanup
- Non-root container user (UID 1001)
- Read-only filesystem with tmpfs for temp files

**Endpoints**:
- `GET /health` - Health check
- `POST /ocr` - Process file with OCR
- `POST /ocr/batch` - Batch processing (future)

**Configuration**:
- TMPDIR for temporary file storage
- Max file size, page limits

## Security Architecture

### Defense in Depth

1. **Network Layer**
   - Services isolated in Docker network
   - No direct external access (behind proxy)
   - Rate limiting at proxy and application level

2. **Application Layer**
   - API key authentication
   - Input validation with Pydantic
   - AST-based safe evaluation (no dynamic code execution)
   - Request size limits
   - CORS restricted to explicit origins

3. **Container Layer**
   - Non-root users (1000, 1001)
   - Read-only root filesystem
   - Capability dropping (`cap_drop: ALL`)
   - Security options (`no-new-privileges`)
   - Resource limits (CPU, memory)
   - Health checks with restart policies

4. **Data Layer**
   - SQLite with WAL mode for concurrency
   - Connection pooling
   - Busy timeout for lock handling
   - Volume isolation

### Authentication & Authorization

- **API Key**: Bearer token in `X-API-Key` header
- **No user management**: Single shared key per environment
- **Future**: Consider OAuth2/JWT for multi-tenant scenarios

### Cryptographic Controls

- **File Integrity**: SHA256 hashing of uploaded files
- **TLS**: Required in production (handled by proxy/ingress)
- **Secrets**: Environment variables, never in code

## Data Architecture

### Brain AI REST Service

**SQLite Database** (`/data/brain_ai.db`):

```sql
-- Documents table
documents (
    id TEXT PRIMARY KEY,          -- Document identifier
    text TEXT NOT NULL,           -- Full text content
    embedding BLOB,               -- Vector embedding
    metadata TEXT,                -- JSON metadata
    created_at TIMESTAMP,
    updated_at TIMESTAMP
)

-- Query cache
query_cache (
    query_hash TEXT PRIMARY KEY,  -- Hash of query
    query TEXT NOT NULL,
    results TEXT NOT NULL,        -- JSON results
    created_at TIMESTAMP,
    accessed_at TIMESTAMP,
    access_count INTEGER
)
```

**Configuration**:
- WAL mode for concurrent reads/writes
- Busy timeout: 5000ms
- Connection pooling (5 connections)
- Synchronous: NORMAL (performance with safety)

### OCR Service

- **No persistent storage**
- Temporary files in `/tmp/ocr` (tmpfs)
- Automatic cleanup after processing

## Observability

### Logging

**Format**: Structured JSON logs
```json
{
  "timestamp": 1699564800.123,
  "level": "INFO",
  "logger": "app.app",
  "message": "Request completed",
  "request_id": "uuid-here",
  "method": "POST",
  "path": "/index",
  "status_code": 200,
  "duration_ms": 45.67
}
```

**Log Levels**:
- DEBUG: Development only
- INFO: Default production
- WARNING: Recoverable issues
- ERROR: Failures requiring attention

**Log Aggregation**: Forward to ELK, Splunk, or CloudWatch

### Metrics

**Format**: Prometheus text format at `/metrics`

**Key Metrics**:
- `brain_ai_requests_total{method, endpoint, status}` - Request counter
- `brain_ai_request_duration_seconds{method, endpoint}` - Request latency histogram
- `brain_ai_documents_indexed_total` - Documents indexed
- `brain_ai_queries_processed_total` - Queries processed
- `brain_ai_calculations_total{status}` - Calculations (success/failure)
- `brain_ai_embedding_latency_seconds{backend}` - Embedding generation time
- `brain_ai_db_operations_total{operation, status}` - Database operations

**Collection**: Prometheus scrapes `/metrics` every 15-30s

### Tracing

**Future Enhancement**: OpenTelemetry integration
- Distributed tracing across services
- Trace IDs in logs and headers
- Export to Jaeger or Zipkin

### Health Checks

- **Liveness** (`/health`): Basic service responsiveness
- **Readiness** (`/ready`): Dependency availability (DB, etc.)
- **Docker**: Built-in health checks with retries

## Deployment

### Local Development

```bash
# Copy environment template
cp .env.example .env

# Edit .env with development settings
# API_KEY=dev-key-not-for-production
# CORS_ORIGINS=http://localhost:3000
# LOG_LEVEL=DEBUG

# Start services
docker compose up -d

# View logs
docker compose logs -f

# Check health
curl http://localhost:8000/health
curl http://localhost:8001/health
```

### Production Deployment

**Prerequisites**:
- Docker 20.10+
- Docker Compose 2.0+
- Reverse proxy (nginx, Traefik, Envoy)
- TLS certificates
- Persistent volumes for data

**Steps**:
1. Generate strong API key: `openssl rand -hex 32`
2. Configure `.env` with production settings
3. Set `ENVIRONMENT=production`
4. Leave `CORS_ORIGINS` empty unless needed
5. Deploy behind TLS-terminating proxy
6. Configure log aggregation
7. Set up Prometheus scraping
8. Configure backup for SQLite volumes

**Kubernetes** (future):
- Helm charts for deployment
- HPA for auto-scaling
- NetworkPolicies for isolation
- PodSecurityPolicies enforced

## CI/CD Pipeline

### GitHub Actions Workflows

1. **CI Tests** (`ci.yml`)
   - Unit tests (pytest)
   - Integration tests
   - Coverage reporting (80% target)
   - Linting (black, isort, flake8)
   - Type checking (mypy)

2. **Security Scanning** (`security-scan.yml`)
   - Bandit (Python security linter)
   - pip-audit (vulnerability scanning)
   - Safety (dependency checking)
   - Trivy (filesystem scanning)
   - CodeQL (static analysis)

3. **Docker Build** (`docker-publish.yml`)
   - Multi-platform builds (amd64, arm64)
   - Trivy container scanning
   - SBOM generation (SPDX)
   - Push to GitHub Container Registry

4. **Dependency Updates** (`dependency-updates.yml`)
   - Monthly automated updates
   - Dependabot for actions, Docker, Python

### Quality Gates

- All tests pass
- No high/critical security findings
- Code coverage > 80%
- Linting passes
- Container scan passes

## Scaling Considerations

### Horizontal Scaling

- **Stateless services**: Scale REST and OCR services independently
- **Load balancing**: Round-robin or least-connections
- **Session affinity**: Not required (stateless)

### Vertical Scaling

- **CPU**: OCR service benefits from more cores
- **Memory**: Embedding models may require 2-4GB
- **Storage**: SQLite suitable for < 100GB; migrate to PostgreSQL beyond

### Database Scaling

**Current (SQLite)**:
- WAL mode supports concurrent readers
- Single writer limitation
- Suitable for < 1000 req/sec

**Future (PostgreSQL/MySQL)**:
- Connection pooling (pgbouncer)
- Read replicas for queries
- Vector extension (pgvector)

### Caching

**Current**: SQLite query cache
**Future**: Redis/Memcached for distributed caching

## Security Runbook

### Incident Response

1. **API Key Compromise**
   - Rotate key immediately
   - Update all environments
   - Review access logs for suspicious activity
   - Consider implementing key rotation schedule

2. **Vulnerability Discovered**
   - Assess severity (CVSS score)
   - Check if exploited (log analysis)
   - Apply patch from dependency update
   - Redeploy services
   - Notify stakeholders

3. **DDoS Attack**
   - Enable rate limiting at proxy
   - Block offending IPs
   - Scale up if needed
   - Contact cloud provider for mitigation

### Regular Maintenance

- **Weekly**: Review security scan results
- **Monthly**: Update dependencies via Dependabot
- **Quarterly**: Rotate API keys
- **Annually**: Security audit and penetration testing

## Backup and Recovery

### Data Backup

**SQLite Database**:
```bash
# Backup with WAL checkpoint
sqlite3 /data/brain_ai.db ".backup /backup/brain_ai_$(date +%Y%m%d).db"

# Or using docker
docker exec brain-ai-rest sqlite3 /data/brain_ai.db ".backup /data/backup.db"
```

**Schedule**: Daily incremental, weekly full

### Disaster Recovery

**RTO** (Recovery Time Objective): < 1 hour
**RPO** (Recovery Point Objective): < 24 hours

**Steps**:
1. Deploy services from Git
2. Restore SQLite backup to volume
3. Update DNS/load balancer
4. Verify health checks
5. Monitor logs for errors

## Future Enhancements

1. **Multi-tenancy**: Per-tenant API keys and data isolation
2. **Vector Database**: Migrate to Pinecone, Weaviate, or Milvus
3. **Advanced OCR**: Integrate full DeepSeek OCR models
4. **Async Processing**: Background jobs with Celery/RQ
5. **API Gateway**: Kong or AWS API Gateway for centralized auth
6. **Monitoring**: Grafana dashboards for metrics
7. **Alerting**: PagerDuty/Opsgenie integration
8. **Secrets Management**: HashiCorp Vault or AWS Secrets Manager

## References

- [FastAPI Documentation](https://fastapi.tiangolo.com/)
- [Pydantic Documentation](https://docs.pydantic.dev/)
- [Prometheus Best Practices](https://prometheus.io/docs/practices/)
- [OWASP API Security](https://owasp.org/www-project-api-security/)
- [12-Factor App](https://12factor.net/)

---

**Last Updated**: 2025-11-07
**Version**: 1.0.0
**Maintained By**: C-AI-BRAIN Team
