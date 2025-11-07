"""Tests for API endpoints."""

import pytest
from fastapi.testclient import TestClient
from app.app import app
from app.config import settings


@pytest.fixture
def client():
    """Create test client."""
    return TestClient(app)


@pytest.fixture
def auth_headers():
    """Get authentication headers."""
    return {"X-API-Key": settings.api_key or "test-key"}


class TestHealthEndpoints:
    """Test health check endpoints."""
    
    def test_health_check(self, client):
        """Test basic health check."""
        response = client.get("/health")
        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "healthy"
        assert "version" in data
        assert "service" in data
    
    def test_healthz_alias(self, client):
        """Test healthz alias."""
        response = client.get("/healthz")
        assert response.status_code == 200
    
    def test_readiness_check(self, client):
        """Test readiness check."""
        response = client.get("/ready")
        assert response.status_code == 200
        data = response.json()
        assert "status" in data
        assert "checks" in data
    
    def test_readyz_alias(self, client):
        """Test readyz alias."""
        response = client.get("/readyz")
        assert response.status_code == 200


class TestAuthenticationn:
    """Test authentication requirements."""
    
    def test_missing_api_key(self, client):
        """Test request without API key."""
        if settings.api_key:  # Only test if auth is required
            response = client.post("/calculate", json={"expression": "2+2"})
            assert response.status_code == 401
    
    def test_invalid_api_key(self, client):
        """Test request with invalid API key."""
        if settings.api_key:
            response = client.post(
                "/calculate",
                json={"expression": "2+2"},
                headers={"X-API-Key": "invalid-key"}
            )
            assert response.status_code == 401
    
    def test_valid_api_key(self, client, auth_headers):
        """Test request with valid API key."""
        response = client.post(
            "/calculate",
            json={"expression": "2+2"},
            headers=auth_headers
        )
        assert response.status_code == 200


class TestCalculateEndpoint:
    """Test calculate endpoint."""
    
    def test_simple_calculation(self, client, auth_headers):
        """Test simple arithmetic."""
        response = client.post(
            "/calculate",
            json={"expression": "2 + 2"},
            headers=auth_headers
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
        assert data["result"] == 4.0
    
    def test_complex_calculation(self, client, auth_headers):
        """Test complex expression."""
        response = client.post(
            "/calculate",
            json={"expression": "sqrt(16) * 3"},
            headers=auth_headers
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
        assert data["result"] == 12.0
    
    def test_invalid_expression(self, client, auth_headers):
        """Test invalid expression."""
        response = client.post(
            "/calculate",
            json={"expression": "import os"},
            headers=auth_headers
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is False
        assert "error" in data
    
    def test_empty_expression(self, client, auth_headers):
        """Test empty expression."""
        response = client.post(
            "/calculate",
            json={"expression": ""},
            headers=auth_headers
        )
        assert response.status_code == 422  # Validation error


class TestIndexEndpoint:
    """Test document indexing endpoint."""
    
    def test_index_document(self, client, auth_headers):
        """Test document indexing."""
        response = client.post(
            "/index",
            json={
                "document_id": "test-doc-1",
                "text": "This is a test document for indexing.",
                "metadata": {"source": "test"}
            },
            headers=auth_headers
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
        assert data["document_id"] == "test-doc-1"
    
    def test_index_empty_text(self, client, auth_headers):
        """Test indexing with empty text."""
        response = client.post(
            "/index",
            json={
                "document_id": "test-doc-2",
                "text": "   ",
            },
            headers=auth_headers
        )
        assert response.status_code == 422  # Validation error
    
    def test_index_long_text(self, client, auth_headers):
        """Test indexing with very long text."""
        long_text = "x" * 200_000  # Exceeds 100K limit
        response = client.post(
            "/index",
            json={
                "document_id": "test-doc-3",
                "text": long_text,
            },
            headers=auth_headers
        )
        assert response.status_code == 422  # Validation error


class TestQueryEndpoint:
    """Test query endpoint."""
    
    def test_query_documents(self, client, auth_headers):
        """Test document query."""
        response = client.post(
            "/query",
            json={
                "query": "test query",
                "top_k": 5
            },
            headers=auth_headers
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
        assert "results" in data
        assert "query_time_ms" in data
    
    def test_query_with_filters(self, client, auth_headers):
        """Test query with filters."""
        response = client.post(
            "/query",
            json={
                "query": "test query",
                "top_k": 10,
                "filters": {"source": "test"}
            },
            headers=auth_headers
        )
        assert response.status_code == 200


class TestMetricsEndpoint:
    """Test metrics endpoint."""
    
    def test_metrics_enabled(self, client):
        """Test metrics when enabled."""
        if settings.metrics_enabled:
            response = client.get("/metrics")
            assert response.status_code == 200
            assert "text/plain" in response.headers["content-type"]
    
    def test_metrics_disabled(self, client, monkeypatch):
        """Test metrics when disabled."""
        monkeypatch.setattr(settings, "metrics_enabled", False)
        response = client.get("/metrics")
        assert response.status_code == 404


class TestRequestValidation:
    """Test request validation and limits."""
    
    def test_request_id_header(self, client):
        """Test request ID is returned in response."""
        response = client.get("/health")
        assert "X-Request-ID" in response.headers
    
    def test_response_time_header(self, client):
        """Test response time header."""
        response = client.get("/health")
        assert "X-Response-Time" in response.headers
        assert "ms" in response.headers["X-Response-Time"]
    
    def test_security_headers(self, client):
        """Test security headers are present."""
        response = client.get("/health")
        assert response.headers["X-Content-Type-Options"] == "nosniff"
        assert response.headers["X-Frame-Options"] == "DENY"
        assert response.headers["X-XSS-Protection"] == "1; mode=block"
        assert "Content-Security-Policy" in response.headers
