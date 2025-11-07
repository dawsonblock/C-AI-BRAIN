# Third-Party Licenses

This document lists all third-party software, libraries, and components used in C-AI-BRAIN, along with their licenses and copyright information.

## Python Dependencies

### brain-ai-rest-service

#### FastAPI
- **Version**: 0.109.x
- **License**: MIT License
- **Copyright**: Copyright (c) 2018 Sebastián Ramírez
- **Repository**: https://github.com/tiangolo/fastapi
- **Purpose**: Web framework for building APIs

#### Uvicorn
- **Version**: 0.27.x
- **License**: BSD 3-Clause License
- **Copyright**: Copyright (c) 2017-present, Encode OSS Ltd.
- **Repository**: https://github.com/encode/uvicorn
- **Purpose**: ASGI server implementation

#### Pydantic
- **Version**: 2.5.x
- **License**: MIT License
- **Copyright**: Copyright (c) 2017-present Samuel Colvin and other contributors
- **Repository**: https://github.com/pydantic/pydantic
- **Purpose**: Data validation using Python type annotations

#### pydantic-settings
- **Version**: 2.1.x
- **License**: MIT License
- **Copyright**: Copyright (c) 2023-present Samuel Colvin and other contributors
- **Repository**: https://github.com/pydantic/pydantic-settings
- **Purpose**: Settings management using Pydantic

#### python-multipart
- **Version**: 0.0.6
- **License**: Apache License 2.0
- **Copyright**: Copyright Andrew Dunham
- **Repository**: https://github.com/andrew-d/python-multipart
- **Purpose**: Multipart form data parsing

#### SlowAPI
- **Version**: 0.1.9
- **License**: MIT License
- **Copyright**: Copyright (c) 2020 Laurent S.
- **Repository**: https://github.com/laurentS/slowapi
- **Purpose**: Rate limiting for FastAPI

#### prometheus-client
- **Version**: 0.19.x
- **License**: Apache License 2.0
- **Copyright**: Copyright 2015 The Prometheus Authors
- **Repository**: https://github.com/prometheus/client_python
- **Purpose**: Prometheus metrics instrumentation

#### httpx
- **Version**: 0.26.x
- **License**: BSD 3-Clause License
- **Copyright**: Copyright (c) 2019, Encode OSS Ltd.
- **Repository**: https://github.com/encode/httpx
- **Purpose**: HTTP client library

### deepseek-ocr-service

Uses the same core dependencies as brain-ai-rest-service (FastAPI, Uvicorn, Pydantic, python-multipart).

## Container Base Images

### Python
- **Base Image**: python:3.11-slim
- **Digest**: sha256:2825cdcfd5753c87f49d7805b9e930a1f3833ac93fe7bc899f1425ee8c8cb36a
- **License**: Python Software Foundation License Version 2
- **Source**: https://github.com/docker-library/python
- **Purpose**: Runtime environment for Python applications

### Tini
- **Version**: Latest from Debian repositories
- **License**: MIT License
- **Copyright**: Copyright (c) 2015-2016 Tianon Gravi
- **Repository**: https://github.com/krallin/tini
- **Purpose**: Init system for containers (proper signal handling)

## Vendored Code

### DeepSeek-OCR (Placeholder)
- **Status**: Not yet integrated
- **License**: [To be determined based on actual implementation]
- **Copyright**: [To be determined]
- **Repository**: [To be specified]
- **Purpose**: OCR processing capabilities
- **Note**: Currently using placeholder implementation. Full integration pending.

### hnswlib (Placeholder)
- **Status**: Not yet integrated
- **License**: Apache License 2.0 (anticipated)
- **Copyright**: [To be determined]
- **Repository**: https://github.com/nmslib/hnswlib
- **Purpose**: Approximate nearest neighbor search for embeddings
- **Note**: Currently not used. Future integration for vector similarity search.

## Development Dependencies

### Testing
- **pytest**: MIT License
- **pytest-asyncio**: Apache License 2.0
- **pytest-cov**: MIT License
- **httpx**: BSD 3-Clause (for testing)

### Code Quality
- **black**: MIT License
- **isort**: MIT License
- **flake8**: MIT License
- **mypy**: MIT License
- **bandit**: Apache License 2.0

### Security Scanning
- **pip-audit**: Apache License 2.0
- **safety**: MIT License

## GitHub Actions

All GitHub Actions used in CI/CD are pinned to specific commit SHAs:

- **actions/checkout@v4**: MIT License
- **actions/setup-python@v5**: MIT License
- **actions/upload-artifact@v4**: MIT License
- **docker/setup-qemu-action@v3**: Apache License 2.0
- **docker/setup-buildx-action@v3**: Apache License 2.0
- **docker/metadata-action@v5**: Apache License 2.0
- **docker/login-action@v3**: Apache License 2.0
- **docker/build-push-action@v6**: Apache License 2.0
- **aquasecurity/trivy-action@v0.24**: Apache License 2.0
- **github/codeql-action@v3**: MIT License
- **anchore/sbom-action@v0.17**: Apache License 2.0
- **codecov/codecov-action@v4**: MIT License
- **peter-evans/create-pull-request@v7**: MIT License

## License Texts

### MIT License

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Apache License 2.0

Full text available at: https://www.apache.org/licenses/LICENSE-2.0

### BSD 3-Clause License

```
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## Compliance Notes

1. **License Compatibility**: All dependencies use permissive licenses (MIT, Apache 2.0, BSD) that are compatible with commercial and open-source use.

2. **Attribution**: When distributing this software, please include this document or equivalent attribution.

3. **Modifications**: Any modifications to third-party code should be clearly documented.

4. **Updates**: When adding new dependencies, update this document with license information.

## Dependency Management

Dependencies are managed through:
- `requirements.txt` - Pinned versions
- Dependabot - Automated updates
- Security scanning - Vulnerability detection

## Questions

For questions about licensing or third-party components, please contact: [contact email]

---

**Last Updated**: 2025-11-07
**Version**: 1.0.0
**Maintained By**: C-AI-BRAIN Team
