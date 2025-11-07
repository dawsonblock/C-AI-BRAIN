"""Tests for OCR service."""

import pytest
from fastapi.testclient import TestClient
from app import app
import io


@pytest.fixture
def client():
    """Create test client."""
    return TestClient(app)


class TestHealthEndpoint:
    """Test health check."""
    
    def test_health_check(self, client):
        """Test health endpoint."""
        response = client.get("/health")
        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "healthy"
        assert "service" in data
        assert "version" in data


class TestOCREndpoint:
    """Test OCR processing endpoint."""
    
    def test_ocr_with_image(self, client):
        """Test OCR with valid image."""
        # Create a simple test image (1x1 pixel PNG)
        image_data = (
            b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01'
            b'\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\x00\x01'
            b'\x00\x00\x05\x00\x01\r\n-\xb4\x00\x00\x00\x00IEND\xaeB`\x82'
        )
        
        files = {"file": ("test.png", io.BytesIO(image_data), "image/png")}
        response = client.post("/ocr", files=files)
        
        assert response.status_code == 200
        data = response.json()
        assert "success" in data
        assert "file_hash" in data
        assert "processing_time_ms" in data
    
    def test_ocr_with_invalid_mode(self, client):
        """Test OCR with invalid mode."""
        image_data = b'\x89PNG\r\n\x1a\n' + b'\x00' * 100
        files = {"file": ("test.png", io.BytesIO(image_data), "image/png")}
        
        response = client.post("/ocr?mode=invalid", files=files)
        assert response.status_code == 400
    
    def test_ocr_with_large_file(self, client):
        """Test OCR with file exceeding size limit."""
        # Create file larger than 50MB
        large_data = b"x" * (51 * 1024 * 1024)
        files = {"file": ("large.png", io.BytesIO(large_data), "image/png")}
        
        response = client.post("/ocr", files=files)
        assert response.status_code == 413
    
    def test_ocr_with_invalid_mime_type(self, client):
        """Test OCR with invalid file type."""
        files = {"file": ("test.exe", io.BytesIO(b"fake exe"), "application/x-msdownload")}
        
        response = client.post("/ocr", files=files)
        assert response.status_code == 400
    
    def test_ocr_modes(self, client):
        """Test different OCR modes."""
        image_data = b'\x89PNG\r\n\x1a\n' + b'\x00' * 100
        
        for mode in ["text", "full", "document", "layout"]:
            files = {"file": ("test.png", io.BytesIO(image_data), "image/png")}
            response = client.post(f"/ocr?mode={mode}", files=files)
            assert response.status_code == 200
    
    def test_file_hash_consistency(self, client):
        """Test that same file produces same hash."""
        image_data = b'\x89PNG\r\n\x1a\n' + b'\x00' * 100
        
        files1 = {"file": ("test.png", io.BytesIO(image_data), "image/png")}
        response1 = client.post("/ocr", files=files1)
        hash1 = response1.json()["file_hash"]
        
        files2 = {"file": ("test.png", io.BytesIO(image_data), "image/png")}
        response2 = client.post("/ocr", files=files2)
        hash2 = response2.json()["file_hash"]
        
        assert hash1 == hash2


class TestBatchOCR:
    """Test batch OCR endpoint."""
    
    def test_batch_not_implemented(self, client):
        """Test batch endpoint returns not implemented."""
        response = client.post("/ocr/batch")
        assert response.status_code == 501
