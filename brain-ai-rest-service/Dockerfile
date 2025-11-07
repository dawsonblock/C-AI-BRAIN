# Production-hardened Dockerfile for brain-ai-rest-service
FROM python:3.11-slim@sha256:2825cdcfd5753c87f49d7805b9e930a1f3833ac93fe7bc899f1425ee8c8cb36a

# Install tini for proper signal handling
RUN apt-get update && \
    apt-get install -y --no-install-recommends tini && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN groupadd -r appuser && \
    useradd -r -g appuser -u 1000 -m -d /home/appuser appuser

# Set working directory
WORKDIR /app

# Copy requirements first for better caching
COPY requirements.txt .

# Install dependencies
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# Copy application code
COPY app/ ./app/

# Create data directory for SQLite
RUN mkdir -p /data && chown appuser:appuser /data

# Switch to non-root user
USER appuser

# Security: Read-only root filesystem (data dir will be mounted)
# Run with: docker run --read-only -v data:/data ...

# Environment variables
ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PATH="/home/appuser/.local/bin:${PATH}"

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8000/health')"

# Expose port
EXPOSE 8000

# Use tini as init system
ENTRYPOINT ["/usr/bin/tini", "--"]

# Run with gunicorn + uvicorn workers
CMD ["python", "-m", "uvicorn", "app.app:app", \
     "--host", "0.0.0.0", \
     "--port", "8000", \
     "--workers", "2", \
     "--log-config", "/dev/null"]
