<div align="center">
  <img src="https://via.placeholder.com/1200x300.png?text=C-AI-BRAIN" alt="C-AI-BRAIN Banner">
  <h1>C-AI-BRAIN 🧠</h1>
  <p>
    <b>Production-ready AI microservices for document processing, semantic search, and secure computation.</b>
  </p>
  <br>
</div>

<div align="center">
  <a href="https://github.com/dawsonblock/C-AI-BRAIN/actions/workflows/security.yml">
    <img src="https://github.com/dawsonblock/C-AI-BRAIN/workflows/Security%20Scanning/badge.svg" alt="Security Scanning">
  </a>
  <a href="https://github.com/dawsonblock/C-AI-BRAIN/actions/workflows/ci.yml">
    <img src="https://github.com/dawsonblock/C-AI-BRAIN/workflows/CI%20Tests/badge.svg" alt="CI Tests">
  </a>
  <a href="https://github.com/dawsonblock/C-AI-BRAIN/actions/workflows/docker-publish.yml">
    <img src="https://github.com/dawsonblock/C-AI-BRAIN/workflows/Docker%20Build%20and%20Publish/badge.svg" alt="Docker Build">
  </a>
</div>

---

## 🚀 Quick Start

**Prerequisites:** Docker 20.10+, Docker Compose 2.0+

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/dawsonblock/C-AI-BRAIN.git
    cd C-AI-BRAIN
    ```

2.  **Configure your environment:**
    ```bash
    cp .env.example .env
    # Generate a secure API key and update .env
    openssl rand -hex 32 >> .env
    ```

3.  **Launch the services:**
    ```bash
    docker compose up -d
    ```

4.  **Test the API:**
    ```bash
    curl -X POST http://localhost:8000/calculate \
      -H "X-API-Key: $(grep API_KEY .env | cut -d '=' -f2)" \
      -H "Content-Type: application/json" \
      -d '{"expression": "2**3 + 4"}'
    ```

---

## ✨ Features

<details>
  <summary><b>Brain AI REST Service</b></summary>

  - **📝 Document Indexing:** Vector embeddings for semantic search.
  - **🔒 Safe Computation:** Secure evaluation of mathematical expressions.
  - **🛡️ API Security:** API key authentication, rate limiting, and CORS controls.
  - **📊 Observability:** Prometheus metrics and structured JSON logging.
  - **🚀 Production Ready:** SQLite in WAL mode, connection pooling, and health checks.
</details>

<details>
  <summary><b>DeepSeek OCR Service</b></summary>

  - **📄 Multi-format Support:** Processes images (JPEG, PNG, etc.) and PDFs.
  - **🔐 Security Controls:** MIME validation, file size limits, and SHA256 hashing.
  - **🔍 Multiple Modes:** Supports text, full, document, and layout extraction.
</details>

---

## 🏗️ Architecture

The C-AI-BRAIN is a microservices-based architecture designed for scalability and security. For a detailed overview, please see the [Architecture Documentation](docs/ARCHITECTURE.md).

---

## 🔧 Development & Testing

<details>
  <summary><b>Local Development</b></summary>

  1.  **Set up a Python environment:**
      ```bash
      python -m venv venv
      source venv/bin/activate
      ```
  2.  **Install dependencies and run the service:**
      ```bash
      cd brain-ai-rest-service
      pip install -r requirements.txt
      uvicorn app.app:app --reload
      ```
</details>

<details>
  <summary><b>Testing</b></summary>

  - **Unit Tests:**
    ```bash
    # From the service directory
    pytest
    ```
  - **Integration Tests:**
    ```bash
    docker compose up -d
    cd tests/integration
    pytest
    ```
</details>

---

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) to get started.

---

## 📜 License

This project is proprietary. All rights reserved.
For details on third-party licenses, see [Third-Party Licenses](THIRD_PARTY_LICENSES.md).
