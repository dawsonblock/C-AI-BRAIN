"""Integration tests for the full system."""

import pytest
import requests
import time
import io


# Test against running docker-compose services
BASE_URL_REST = "http://localhost:8000"
BASE_URL_OCR = "http://localhost:8001"
API_KEY = "test-key-for-ci"


@pytest.fixture(scope="module")
def wait_for_services():
    """Wait for services to be ready."""
    max_attempts = 30
    for service_name, url in [("REST", BASE_URL_REST), ("OCR", BASE_URL_OCR)]:
        for attempt in range(max_attempts):
            try:
                response = requests.get(f"{url}/health", timeout=2)
                if response.status_code == 200:
                    print(f"{service_name} service ready")
                    break
            except requests.RequestException:
                if attempt == max_attempts - 1:
                    pytest.skip(f"{service_name} service not available")
                time.sleep(2)


class TestSystemIntegration:
    """Test full system integration."""
    
    def test_rest_service_health(self, wait_for_services):
        """Test REST service health."""
        response = requests.get(f"{BASE_URL_REST}/health")
        assert response.status_code == 200
        assert response.json()["status"] == "healthy"
    
    def test_ocr_service_health(self, wait_for_services):
        """Test OCR service health."""
        response = requests.get(f"{BASE_URL_OCR}/health")
        assert response.status_code == 200
        assert response.json()["status"] == "healthy"
    
    def test_rest_authentication(self, wait_for_services):
        """Test REST authentication works."""
        # Without key
        response = requests.post(
            f"{BASE_URL_REST}/calculate",
            json={"expression": "2+2"}
        )
        assert response.status_code == 401
        
        # With key
        response = requests.post(
            f"{BASE_URL_REST}/calculate",
            json={"expression": "2+2"},
            headers={"X-API-Key": API_KEY}
        )
        assert response.status_code == 200
    
    def test_calculate_endpoint(self, wait_for_services):
        """Test calculation endpoint."""
        response = requests.post(
            f"{BASE_URL_REST}/calculate",
            json={"expression": "sqrt(16) * 2"},
            headers={"X-API-Key": API_KEY}
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
        assert data["result"] == 8.0
    
    def test_document_indexing(self, wait_for_services):
        """Test document indexing."""
        response = requests.post(
            f"{BASE_URL_REST}/index",
            json={
                "document_id": "integration-test-1",
                "text": "This is an integration test document.",
                "metadata": {"test": True}
            },
            headers={"X-API-Key": API_KEY}
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
    
    def test_document_query(self, wait_for_services):
        """Test document query."""
        response = requests.post(
            f"{BASE_URL_REST}/query",
            json={
                "query": "integration test",
                "top_k": 5
            },
            headers={"X-API-Key": API_KEY}
        )
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
        assert "results" in data
    
    def test_ocr_processing(self, wait_for_services):
        """Test OCR processing."""
        # Simple 1x1 PNG
        image_data = (
            b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01'
            b'\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\x00\x01'
            b'\x00\x00\x05\x00\x01\r\n-\xb4\x00\x00\x00\x00IEND\xaeB`\x82'
        )
        
        files = {"file": ("test.png", io.BytesIO(image_data), "image/png")}
        response = requests.post(f"{BASE_URL_OCR}/ocr", files=files)
        
        assert response.status_code == 200
        data = response.json()
        assert data["success"] is True
        assert "file_hash" in data
    
    def test_metrics_endpoint(self, wait_for_services):
        """Test metrics are available."""
        response = requests.get(f"{BASE_URL_REST}/metrics")
        assert response.status_code == 200
        assert "brain_ai" in response.text
    
    def test_security_headers(self, wait_for_services):
        """Test security headers are present."""
        response = requests.get(f"{BASE_URL_REST}/health")
        assert "X-Content-Type-Options" in response.headers
        assert response.headers["X-Content-Type-Options"] == "nosniff"
        assert "X-Frame-Options" in response.headers
        assert "Content-Security-Policy" in response.headers
    
    def test_request_id_propagation(self, wait_for_services):
        """Test request ID is returned."""
        response = requests.get(f"{BASE_URL_REST}/health")
        assert "X-Request-ID" in response.headers
        
        # Test custom request ID
        custom_id = "test-request-123"
        response = requests.get(
            f"{BASE_URL_REST}/health",
            headers={"X-Request-ID": custom_id}
        )
        assert response.headers["X-Request-ID"] == custom_id
