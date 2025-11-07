# Production-hardened Dockerfile for deepseek-ocr-service
FROM python:3.11-slim@sha256:2825cdcfd5753c87f49d7805b9e930a1f3833ac93fe7bc899f1425ee8c8cb36a

# Install system dependencies and tini
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        tini \
        libmagic1 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN groupadd -r appuser && \
    useradd -r -g appuser -u 1001 -m -d /home/appuser appuser

# Set working directory
WORKDIR /app

# Copy requirements
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# Copy application
COPY app.py .

# Create temp directory with proper permissions
RUN mkdir -p /tmp/ocr && chown appuser:appuser /tmp/ocr

# Switch to non-root user
USER appuser

# Environment variables
ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    TMPDIR=/tmp/ocr

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8001/health')"

# Expose port
EXPOSE 8001

# Use tini
ENTRYPOINT ["/usr/bin/tini", "--"]

# Run application
CMD ["python", "-m", "uvicorn", "app:app", \
     "--host", "0.0.0.0", \
     "--port", "8001", \
     "--workers", "2"]
