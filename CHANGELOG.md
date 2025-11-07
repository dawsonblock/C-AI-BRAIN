# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-11-07

### Added - Initial Production Release

#### Brain AI REST Service
- FastAPI-based REST API with async support
- API key authentication on all sensitive endpoints
- Safe mathematical expression evaluation (AST-based, no code execution)
- Document indexing with vector embeddings
- Semantic search over indexed documents
- SQLite database with WAL mode for concurrency
- Connection pooling for database operations
- Prometheus metrics endpoint
- Structured JSON logging with request IDs
- Rate limiting (100 req/min default)
- CORS with explicit origin allowlist
- Request size limits (1MB default)
- Security headers (CSP, X-Frame-Options, HSTS)
- Health and readiness checks
- Pydantic v2 request/response validation

#### DeepSeek OCR Service
- OCR processing for images and PDFs
- MIME type validation with allowlist
- File size limits (50MB max)
- SHA256 hashing for file traceability
- PDF page count limits (100 pages max)
- Multiple processing modes (text, full, document, layout)
- Temporary file cleanup
- Health checks

#### Infrastructure
- Production-hardened Dockerfiles
  - Non-root users (UID 1000, 1001)
  - Read-only root filesystem
  - Capability dropping
  - Tini init system
  - SHA-pinned base images
- Docker Compose with
  - Resource limits (CPU, memory)
  - Security options (no-new-privileges, seccomp)
  - Health checks with retries
  - Isolated network
  - Volume management
  - Structured logging

#### CI/CD
- GitHub Actions workflows
  - Security scanning (Bandit, pip-audit, Safety, Trivy, CodeQL)
  - Unit and integration tests
  - Multi-platform Docker builds (amd64, arm64)
  - SBOM generation
  - Automated dependency updates
- Dependabot for GitHub Actions, Docker, and Python dependencies
- All actions pinned to commit SHAs

#### Testing
- Comprehensive unit test suite (80%+ coverage target)
- Integration tests for full system
- Async tests for FastAPI endpoints
- Property-based tests for safe evaluation
- Database operation tests
- Security control tests

#### Documentation
- Complete architecture documentation
- Security policy and best practices
- Third-party license compliance
- API documentation (auto-generated)
- Deployment guides
- Development setup instructions
- Configuration reference

### Security
- Eliminated all unsafe `eval()` and `exec()` usage
- Implemented AST-based safe expression evaluation
- Removed subprocess-based code execution
- Added comprehensive input validation
- Enforced API key authentication
- Disabled CORS by default (explicit opt-in)
- Added request size limits
- Implemented rate limiting
- Added security headers
- Container hardening (non-root, read-only, dropped capabilities)
- Supply chain security (pinned dependencies, automated scanning)
- SHA256 file hashing for uploads

### Fixed
- SQLite concurrency issues (WAL mode, busy timeout)
- Connection pooling for better performance
- Request validation and error handling
- Security header implementation
- Proper signal handling in containers (tini)

---

## [Unreleased]

### Planned
- Multi-tenancy support
- PostgreSQL migration option
- Advanced vector database integration
- Full DeepSeek OCR model integration
- Async job processing
- GraphQL API
- Kubernetes Helm charts
- Enhanced monitoring dashboards
- Webhook support

---

**Note**: This is the initial production-ready release with all critical security fixes applied.
