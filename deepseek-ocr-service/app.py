#!/usr/bin/env python3
"""
DeepSeek-OCR FastAPI Service
Provides OCR endpoints compatible with Brain-AI C++ client
"""

from fastapi import FastAPI, File, UploadFile, Form, HTTPException
# from typing import Optional, Dict, Any
import logging
import time
from datetime import datetime

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

app = FastAPI(
    title="DeepSeek-OCR Service",
    description="OCR service for Brain-AI document processing pipeline",
    version="1.0.0"
)

# Service metadata
SERVICE_INFO = {
    "name": "deepseek-ocr-service",
    "version": "1.0.0",
    "model": "deepseek-ocr-base",
    "status": "ready",
    "started_at": datetime.now().isoformat()
}

# Statistics tracking
stats = {
    "total_requests": 0,
    "successful_requests": 0,
    "failed_requests": 0,
    "total_processing_time_ms": 0
}


@app.get("/")
async def root():
    """Root endpoint - service information"""
    return {
        "service": SERVICE_INFO["name"],
        "version": SERVICE_INFO["version"],
        "status": "healthy",
        "endpoints": {
            "health": "/health",
            "status": "/status",
            "extract": "/ocr/extract",
            "stats": "/stats"
        }
    }


@app.get("/health")
async def health_check():
    """Health check endpoint for monitoring"""
    return {
        "status": "healthy",
        "timestamp": datetime.now().isoformat()
    }


@app.get("/status")
async def service_status():
    """Detailed service status"""
    return {
        "status": SERVICE_INFO["status"],
        "version": SERVICE_INFO["version"],
        "model": SERVICE_INFO["model"],
        "started_at": SERVICE_INFO["started_at"],
        "uptime_seconds": (datetime.now() - datetime.fromisoformat(SERVICE_INFO["started_at"])).total_seconds(),
        "statistics": stats
    }


@app.get("/stats")
async def get_stats():
    """Service statistics"""
    avg_time = (
        stats["total_processing_time_ms"] / stats["total_requests"]
        if stats["total_requests"] > 0
        else 0
    )
    
    return {
        "total_requests": stats["total_requests"],
        "successful_requests": stats["successful_requests"],
        "failed_requests": stats["failed_requests"],
        "success_rate": (
            stats["successful_requests"] / stats["total_requests"]
            if stats["total_requests"] > 0
            else 0
        ),
        "average_processing_time_ms": avg_time
    }


@app.post("/ocr/extract")
async def extract_text(
    file: UploadFile = File(...),
    mode: str = Form("base"),
    task: str = Form("ocr"),
    max_tokens: int = Form(8192),
    temperature: float = Form(0.0)
):
    """
    Extract text from document using OCR
    
    Parameters:
    - file: Document file (PDF, image, etc.)
    - mode: OCR mode (tiny, small, base, large, gundam)
    - task: Processing task (ocr, markdown, figure, reference, describe)
    - max_tokens: Maximum tokens to generate
    - temperature: Sampling temperature
    
    Returns:
    - text: Extracted text content
    - confidence: Confidence score (0.0-1.0)
    - metadata: Processing metadata
    """
    start_time = time.time()
    stats["total_requests"] += 1
    
    try:
        logger.info(f"Processing file: {file.filename} (mode={mode}, task={task})")
        
        # Read file content
        content = await file.read()
        file_size = len(content)
        
        logger.info(f"File size: {file_size} bytes")
        
        # Validate file
        if file_size == 0:
            raise HTTPException(status_code=400, detail="Empty file")
        
        if file_size > 50 * 1024 * 1024:  # 50MB limit
            raise HTTPException(status_code=413, detail="File too large (max 50MB)")
        
        # Validate parameters
        valid_modes = ["tiny", "small", "base", "large", "gundam"]
        if mode not in valid_modes:
            raise HTTPException(
                status_code=400,
                detail=f"Invalid mode. Must be one of: {', '.join(valid_modes)}"
            )
        
        valid_tasks = ["ocr", "markdown", "figure", "reference", "describe"]
        if task not in valid_tasks:
            raise HTTPException(
                status_code=400,
                detail=f"Invalid task. Must be one of: {', '.join(valid_tasks)}"
            )
        
        # TODO: Integrate actual DeepSeek OCR model here
        # For now, return mock response with realistic processing
        
        # Simulate processing time based on file size and mode
        processing_time = simulate_processing_time(file_size, mode)
        time.sleep(processing_time / 1000.0)  # Convert to seconds
        
        # Generate mock OCR result
        extracted_text = generate_mock_ocr_result(file.filename, task, file_size)
        
        # Calculate confidence (mock - would come from actual OCR model)
        confidence = calculate_mock_confidence(file_size, mode)
        
        # Prepare response
        processing_time_ms = int((time.time() - start_time) * 1000)
        stats["total_processing_time_ms"] += processing_time_ms
        stats["successful_requests"] += 1
        
        response = {
            "success": True,  # ✅ Added for C++ client compatibility
            "text": extracted_text,
            "confidence": confidence,
            "error_message": "",  # Empty when successful
            "processing_time_ms": processing_time_ms,  # ✅ Moved to top level for C++ client
            "metadata": {
                "filename": file.filename,
                "file_size_bytes": file_size,
                "mode": mode,
                "task": task,
                "max_tokens": max_tokens,
                "temperature": temperature,
                "model": SERVICE_INFO["model"],
                "timestamp": datetime.now().isoformat()
            }
        }
        
        logger.info(
            f"Successfully processed {file.filename}: "
            f"{len(extracted_text)} chars, "
            f"confidence={confidence:.2f}, "
            f"time={processing_time_ms}ms"
        )
        
        return response
        
    except HTTPException:
        stats["failed_requests"] += 1
        raise
        
    except Exception as e:
        stats["failed_requests"] += 1
        logger.error(f"Error processing file {file.filename}: {str(e)}")
        raise HTTPException(
            status_code=500,
            detail=f"Internal server error: {str(e)}"
        )


def simulate_processing_time(file_size: int, mode: str) -> int:
    """Simulate realistic processing time based on file size and mode"""
    base_time = {
        "tiny": 100,
        "small": 200,
        "base": 300,
        "large": 500,
        "gundam": 800
    }
    
    # Base time + size-dependent component
    size_factor = min(file_size / (1024 * 1024), 10)  # Max 10MB contribution
    total_time = base_time.get(mode, 300) + int(size_factor * 50)
    
    return total_time


def calculate_mock_confidence(file_size: int, mode: str) -> float:
    """Calculate mock confidence score"""
    # Better modes = higher confidence
    mode_confidence = {
        "tiny": 0.75,
        "small": 0.82,
        "base": 0.88,
        "large": 0.93,
        "gundam": 0.97
    }
    
    base_conf = mode_confidence.get(mode, 0.85)
    
    # Larger files = slightly lower confidence (more complex)
    size_penalty = min(file_size / (10 * 1024 * 1024), 0.1)
    
    return max(0.7, min(0.99, base_conf - size_penalty))


def generate_mock_ocr_result(filename: str, task: str, file_size: int) -> str:
    """Generate mock OCR result based on task type"""
    
    if task == "ocr":
        return f"""This is a sample document extracted from {filename}.

The document contains text content that has been processed using optical character recognition.
The file size is approximately {file_size / 1024:.1f} KB.

This is a mock implementation that simulates the OCR extraction process.
In production, this would be replaced with actual DeepSeek OCR model inference.

Key features of the document:
- Clean text extraction
- Proper spacing and formatting
- High confidence recognition
- Metadata preservation

The OCR system can handle various document types including:
• PDF documents
• Scanned images
• Screenshots
• Multi-page documents
• Complex layouts

For production deployment, integrate the actual DeepSeek OCR model here."""

    elif task == "markdown":
        return f"""# Document: {filename}

## Summary
This document has been processed using markdown extraction mode.

## Content Structure

### Section 1: Introduction
The document contains structured content extracted from the source file.

### Section 2: Main Content
- **File Size**: {file_size / 1024:.1f} KB
- **Format**: Markdown
- **Processing**: Automatic

### Section 3: Key Points
1. First key point from the document
2. Second important observation
3. Third critical insight

## Conclusion
This markdown extraction preserves document structure and formatting.

---
*Generated by DeepSeek-OCR Service*"""

    elif task == "figure":
        return f"""[Figure Description]

Figure extracted from {filename}:

The image contains visual content that has been analyzed and described.
Key visual elements:
- Primary content: Document layout
- Secondary elements: Text blocks, images, diagrams
- Overall composition: Structured document format

Visual quality assessment:
- Resolution: Adequate for text extraction
- Clarity: Good readability
- Color: Standard document coloring

[End Figure Description]"""

    elif task == "reference":
        return f"""References extracted from {filename}:

[1] Primary source document - {filename}
    Size: {file_size} bytes
    Type: Digital document

[2] Processing metadata
    Service: DeepSeek-OCR Service v1.0.0
    Mode: Reference extraction
    Timestamp: {datetime.now().isoformat()}

[3] Related content
    Content type: Structured text
    Format: Reference list
    Status: Extracted

Note: This is a mock reference extraction.
In production, actual citations and references would be extracted."""

    else:  # describe
        return f"""Document Description for {filename}:

Type: Digital document
Size: {file_size / 1024:.1f} KB
Format: Binary file

Content Overview:
The document appears to be a standard text-based file suitable for OCR processing.
It contains structured content that can be extracted and processed.

Processing Recommendations:
- Use 'ocr' mode for plain text extraction
- Use 'markdown' mode for structured content
- Use 'figure' mode for image analysis
- Use 'reference' mode for citation extraction

Quality Assessment:
- Text clarity: Good
- Layout complexity: Moderate
- Recommended processing mode: base or large

This description is generated by the DeepSeek-OCR service mock implementation."""


if __name__ == "__main__":
    import uvicorn
    
    logger.info("Starting DeepSeek-OCR Service...")
    logger.info(f"Service info: {SERVICE_INFO}")
    
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000,
        log_level="info",
        access_log=True
    )
