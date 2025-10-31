"""
DeepSeek-OCR FastAPI Service
Provides OCR and document processing capabilities via REST API
"""
import io
import time
import logging
from typing import List, Optional, Dict, Any
from contextlib import asynccontextmanager

from fastapi import FastAPI, File, UploadFile, HTTPException, Form
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from PIL import Image

from app.config import Settings
from app.ocr_engine import OCREngine

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Global OCR engine instance
ocr_engine: Optional[OCREngine] = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifespan context manager for startup/shutdown"""
    global ocr_engine
    
    # Startup
    logger.info("Initializing DeepSeek-OCR service...")
    settings = Settings()
    try:
        ocr_engine = OCREngine(settings)
        logger.info(f"OCR Engine initialized with model: {settings.model_name}")
        logger.info(f"Resolution: {settings.default_resolution}, Device: {settings.device}")
    except Exception as e:
        logger.error(f"Failed to initialize OCR engine: {e}")
        raise
    
    yield
    
    # Shutdown
    logger.info("Shutting down DeepSeek-OCR service...")
    if ocr_engine:
        ocr_engine.cleanup()


app = FastAPI(
    title="DeepSeek-OCR Service",
    description="High-fidelity OCR and document processing service",
    version="1.0.0",
    lifespan=lifespan
)


# ============================================================================
# Request/Response Models
# ============================================================================

class OCRRequest(BaseModel):
    """OCR extraction request parameters"""
    mode: str = Field(default="base", description="Resolution mode: tiny, small, base, large, gundam")
    task: str = Field(default="markdown", description="Task type: ocr, markdown, figure, reference, describe")
    max_tokens: int = Field(default=8192, description="Maximum output tokens")
    temperature: float = Field(default=0.0, description="Sampling temperature")


class OCRResponse(BaseModel):
    """OCR extraction response"""
    text: str
    confidence: float
    processing_time_ms: float
    metadata: Dict[str, Any]
    success: bool
    error_message: Optional[str] = None


class BatchOCRResponse(BaseModel):
    """Batch OCR response"""
    results: List[OCRResponse]
    total_files: int
    successful: int
    failed: int
    total_time_ms: float


class HealthResponse(BaseModel):
    """Health check response"""
    status: str
    model_loaded: bool
    device: str
    resolution: str
    uptime_seconds: float


# ============================================================================
# API Endpoints
# ============================================================================

@app.get("/health", response_model=HealthResponse)
async def health_check():
    """Health check endpoint"""
    if not ocr_engine:
        raise HTTPException(status_code=503, detail="OCR engine not initialized")
    
    return HealthResponse(
        status="healthy",
        model_loaded=ocr_engine.is_loaded(),
        device=ocr_engine.settings.device,
        resolution=ocr_engine.settings.default_resolution,
        uptime_seconds=time.time() - ocr_engine.start_time
    )


@app.post("/ocr/extract", response_model=OCRResponse)
async def extract_text(
    file: UploadFile = File(...),
    mode: str = Form("base"),
    task: str = Form("markdown"),
    max_tokens: int = Form(8192),
    temperature: float = Form(0.0)
):
    """
    Extract text from an image or document
    
    Args:
        file: Image file (PNG, JPEG, PDF)
        mode: Resolution mode (tiny, small, base, large, gundam)
        task: Task type (ocr, markdown, figure, reference, describe)
        max_tokens: Maximum output tokens
        temperature: Sampling temperature
    
    Returns:
        OCRResponse with extracted text and metadata
    """
    if not ocr_engine:
        raise HTTPException(status_code=503, detail="OCR engine not initialized")
    
    start_time = time.time()
    
    try:
        # Read and validate image
        contents = await file.read()
        image = Image.open(io.BytesIO(contents)).convert("RGB")
        
        # Validate parameters
        if mode not in ["tiny", "small", "base", "large", "gundam"]:
            raise HTTPException(status_code=400, detail=f"Invalid mode: {mode}")
        
        if task not in ["ocr", "markdown", "figure", "reference", "describe"]:
            raise HTTPException(status_code=400, detail=f"Invalid task: {task}")
        
        # Process with OCR engine
        result = ocr_engine.process(
            image=image,
            mode=mode,
            task=task,
            max_tokens=max_tokens,
            temperature=temperature
        )
        
        processing_time = (time.time() - start_time) * 1000
        
        return OCRResponse(
            text=result["text"],
            confidence=result.get("confidence", 0.0),
            processing_time_ms=processing_time,
            metadata={
                "mode": mode,
                "task": task,
                "filename": file.filename,
                "image_size": image.size,
                "tokens_generated": result.get("tokens_generated", 0)
            },
            success=True
        )
        
    except Exception as e:
        logger.error(f"OCR extraction failed: {e}", exc_info=True)
        processing_time = (time.time() - start_time) * 1000
        
        return OCRResponse(
            text="",
            confidence=0.0,
            processing_time_ms=processing_time,
            metadata={"filename": file.filename},
            success=False,
            error_message=str(e)
        )


@app.post("/ocr/batch", response_model=BatchOCRResponse)
async def batch_extract(
    files: List[UploadFile] = File(...),
    mode: str = Form("base"),
    task: str = Form("markdown"),
    max_tokens: int = Form(8192),
    temperature: float = Form(0.0)
):
    """
    Batch process multiple documents
    
    Args:
        files: List of image files
        mode: Resolution mode
        task: Task type
        max_tokens: Maximum output tokens per document
        temperature: Sampling temperature
    
    Returns:
        BatchOCRResponse with results for all files
    """
    if not ocr_engine:
        raise HTTPException(status_code=503, detail="OCR engine not initialized")
    
    start_time = time.time()
    results = []
    successful = 0
    failed = 0
    
    for file in files:
        try:
            # Process each file
            contents = await file.read()
            image = Image.open(io.BytesIO(contents)).convert("RGB")
            
            file_start = time.time()
            result = ocr_engine.process(
                image=image,
                mode=mode,
                task=task,
                max_tokens=max_tokens,
                temperature=temperature
            )
            file_time = (time.time() - file_start) * 1000
            
            results.append(OCRResponse(
                text=result["text"],
                confidence=result.get("confidence", 0.0),
                processing_time_ms=file_time,
                metadata={
                    "mode": mode,
                    "task": task,
                    "filename": file.filename,
                    "image_size": image.size
                },
                success=True
            ))
            successful += 1
            
        except Exception as e:
            logger.error(f"Failed to process {file.filename}: {e}")
            results.append(OCRResponse(
                text="",
                confidence=0.0,
                processing_time_ms=0.0,
                metadata={"filename": file.filename},
                success=False,
                error_message=str(e)
            ))
            failed += 1
    
    total_time = (time.time() - start_time) * 1000
    
    return BatchOCRResponse(
        results=results,
        total_files=len(files),
        successful=successful,
        failed=failed,
        total_time_ms=total_time
    )


@app.get("/")
async def root():
    """Root endpoint"""
    return {
        "service": "DeepSeek-OCR",
        "version": "1.0.0",
        "status": "running",
        "endpoints": {
            "health": "/health",
            "extract": "/ocr/extract",
            "batch": "/ocr/batch"
        }
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
