#!/usr/bin/env python3
"""
Brain-AI REST Service - Real C++ Integration
REST API wrapper using actual C++ library via pybind11 bindings
"""

from fastapi import FastAPI, HTTPException, File, UploadFile
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from typing import List, Optional, Dict, Any
import logging
import time
import asyncio
from datetime import datetime
import sys
import os

# Try to import brain_ai_py (C++ bindings)
try:
    import brain_ai_py
    BINDINGS_AVAILABLE = True
    logger = logging.getLogger(__name__)
    logger.info(f"✅ brain_ai_py loaded successfully - version {brain_ai_py.__version__}")
except ImportError as e:
    BINDINGS_AVAILABLE = False
    logger = logging.getLogger(__name__)
    logger.warning(f"⚠️  brain_ai_py not available: {e}")
    logger.warning("Falling back to mock implementation")
    logger.warning("To build bindings: cd brain-ai && pip install -e .")

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)

app = FastAPI(
    title="Brain-AI REST Service",
    description="REST API for Brain-AI document processing and cognitive query system",
    version="1.0.0"
)

# Service metadata
SERVICE_INFO = {
    "name": "brain-ai-rest-service",
    "version": "1.0.0",
    "bindings_available": BINDINGS_AVAILABLE,
    "started_at": datetime.now().isoformat()
}

# Statistics tracking
import threading

stats = {
    "total_documents": 0,
    "successful_documents": 0,
    "failed_documents": 0,
    "total_queries": 0,
    "successful_queries": 0,
    "failed_queries": 0,
    "total_document_time_ms": 0,
    "total_query_time_ms": 0
}
stats_lock = threading.Lock()

# ==================== Request/Response Models ====================

class OCRConfig(BaseModel):
    service_url: str = "http://localhost:8000"
    mode: str = "base"
    task: str = "ocr"
    max_tokens: int = 8192
    temperature: float = 0.0

class DocumentProcessRequest(BaseModel):
    doc_id: str
    file_path: Optional[str] = None
    content: Optional[str] = None  # Pre-extracted text
    extract_text: bool = True
    index_document: bool = True
    ocr_config: Optional[OCRConfig] = OCRConfig()

class DocumentProcessResponse(BaseModel):
    success: bool
    doc_id: str
    extracted_text: str = ""
    ocr_confidence: float = 0.0
    indexed: bool = False
    processing_time_ms: int = 0
    error_message: str = ""

class BatchDocumentRequest(BaseModel):
    documents: List[DocumentProcessRequest]

class BatchDocumentResponse(BaseModel):
    total: int
    successful: int
    failed: int
    results: List[DocumentProcessResponse]
    processing_time_ms: int

class QueryRequest(BaseModel):
    query: str
    query_embedding: List[float]
    top_k: int = 5
    use_episodic: bool = True
    use_semantic: bool = True

class ScoredResult(BaseModel):
    content: str
    score: float
    source: str

class QueryResponse(BaseModel):
    query: str
    response: str
    confidence: float
    results: List[ScoredResult]
    explanation: Dict[str, Any] = {}
    processing_time_ms: int

class SearchRequest(BaseModel):
    query_embedding: List[float]
    top_k: int = 10

class SearchResult(BaseModel):
    doc_id: str
    content: str
    similarity: float
    metadata: Dict[str, Any] = {}

class SearchResponse(BaseModel):
    results: List[SearchResult]
    total_results: int

class IndexRequest(BaseModel):
    doc_id: str
    embedding: List[float]
    content: str
    metadata: Dict[str, Any] = {}

class IndexResponse(BaseModel):
    success: bool
    doc_id: str
    indexed: bool
    error_message: str = ""

class EpisodeRequest(BaseModel):
    content: str
    embedding: List[float]
    importance: float = 1.0

class EpisodeResponse(BaseModel):
    success: bool
    error_message: str = ""

class HealthResponse(BaseModel):
    status: str
    version: str
    timestamp: str
    bindings_available: bool
    components: Dict[str, str]

class StatsResponse(BaseModel):
    total_documents: int
    successful_documents: int
    total_queries: int
    successful_queries: int
    episodic_count: int
    vector_index_size: int
    semantic_entity_count: int
    semantic_relation_count: int
    uptime_seconds: float
    avg_query_time_ms: float
    avg_document_time_ms: float

# ==================== Brain-AI Backend (Real C++ Integration) ====================

class RealBrainAI:
    """Real implementation using C++ bindings"""
    
    def __init__(self):
        if not BINDINGS_AVAILABLE:
            raise RuntimeError("brain_ai_py bindings not available")
        
        # Initialize C++ cognitive handler
        self.fusion_weights = brain_ai_py.FusionWeights(
            vector=0.5,
            episodic=0.3,
            semantic=0.2
        )
        
        self.handler = brain_ai_py.CognitiveHandler(
            episodic_capacity=128,
            fusion_weights=self.fusion_weights,
            embedding_dim=1536  # Default OpenAI embedding dimension
        )
        
        logger.info("✅ RealBrainAI initialized with C++ cognitive handler")
        logger.info(f"   Configuration: episodic_capacity=128, embedding_dim=1536")
    
    async def process_document(self, request: DocumentProcessRequest) -> DocumentProcessResponse:
        """Process document with real C++ integration"""
        start_time = time.time()
        
        try:
            # Extract text (via OCR or use provided content)
            if request.content:
                extracted_text = request.content
                ocr_confidence = 1.0
            elif request.extract_text and request.file_path:
                # Call OCR service
                import httpx
                async with httpx.AsyncClient() as client:
                    with open(request.file_path, 'rb') as f:
                        files = {'file': f}
                        data = {
                            'mode': request.ocr_config.mode,
                            'task': request.ocr_config.task
                        }
                        response = await client.post(
                            f"{request.ocr_config.service_url}/ocr/extract",
                            files=files,
                            data=data
                        )
                        response.raise_for_status()
                        ocr_result = response.json()
                        extracted_text = ocr_result.get('text', '')
                        ocr_confidence = ocr_result.get('confidence', 0.0)
            else:
                extracted_text = f"Document {request.doc_id}"
                ocr_confidence = 0.0
            
            # Index document if requested
            indexed = False
            if request.index_document and extracted_text:
                # Generate embedding (mock for now - would use real embeddings)
                embedding = [0.1] * 1536  # TODO: Replace with real embedding
                
                # Index via C++ handler
                self.handler.index_document(
                    doc_id=request.doc_id,
                    embedding=embedding,
                    content=extracted_text,
                    metadata={}
                )
                indexed = True
            
            processing_time_ms = int((time.time() - start_time) * 1000)
            
            return DocumentProcessResponse(
                success=True,
                doc_id=request.doc_id,
                extracted_text=extracted_text,
                ocr_confidence=ocr_confidence,
                indexed=indexed,
                processing_time_ms=processing_time_ms
            )
            
        except Exception as e:
            logger.error(f"Error processing document {request.doc_id}: {e}")
            return DocumentProcessResponse(
                success=False,
                doc_id=request.doc_id,
                error_message=str(e),
                processing_time_ms=int((time.time() - start_time) * 1000)
            )
    
    async def process_query(self, request: QueryRequest) -> QueryResponse:
        """Process query with real C++ cognitive handler"""
        start_time = time.time()
        
        try:
            # Call C++ process_query
            result_dict = self.handler.process_query(
                query=request.query,
                query_embedding=request.query_embedding
            )
            
            processing_time_ms = int((time.time() - start_time) * 1000)
            
            # Convert results
            results = []
            for r in result_dict['results'][:request.top_k]:
                results.append(ScoredResult(
                    content=r['content'],
                    score=r['score'],
                    source=r['source']
                ))
            
            return QueryResponse(
                query=result_dict['query'],
                response=result_dict['response'],
                confidence=result_dict['confidence'],
                results=results,
                explanation=result_dict['explanation'],
                processing_time_ms=processing_time_ms
            )
            
        except Exception as e:
            logger.error(f"Error processing query: {e}")
            raise HTTPException(status_code=500, detail=str(e))
    
    def get_stats(self) -> Dict[str, Any]:
        """Get statistics from C++ handler"""
        try:
            cpp_stats = self.handler.get_stats()
            return {
                "episodic_count": cpp_stats['episodic_count'],
                "vector_index_size": cpp_stats['vector_index_size'],
                "semantic_entity_count": cpp_stats['semantic_entity_count'],
                "semantic_relation_count": cpp_stats['semantic_relation_count']
            }
        except Exception as e:
            logger.error(f"Error getting stats: {e}")
            return {
                "episodic_count": 0,
                "vector_index_size": 0,
                "semantic_entity_count": 0,
                "semantic_relation_count": 0
            }

# ==================== Fallback Mock Implementation ====================

class MockBrainAI:
    """Mock implementation when bindings are not available"""
    
    def __init__(self):
        self.documents = {}
        self.episodes = []
        logger.info("⚠️  MockBrainAI initialized (fallback mode)")
    
    async def process_document(self, request: DocumentProcessRequest) -> DocumentProcessResponse:
        start_time = time.time()
        
        try:
            extracted_text = f"Mock extracted text from {request.doc_id}"
            
            if request.index_document:
                self.documents[request.doc_id] = {
                    "text": extracted_text,
                    "indexed_at": datetime.now().isoformat()
                }
            
            processing_time_ms = int((time.time() - start_time) * 1000)
            
            return DocumentProcessResponse(
                success=True,
                doc_id=request.doc_id,
                extracted_text=extracted_text,
                ocr_confidence=0.85,
                indexed=request.index_document,
                processing_time_ms=processing_time_ms
            )
            
        except Exception as e:
            return DocumentProcessResponse(
                success=False,
                doc_id=request.doc_id,
                error_message=str(e),
                processing_time_ms=int((time.time() - start_time) * 1000)
            )
    
    async def process_query(self, request: QueryRequest) -> QueryResponse:
        start_time = time.time()
        
        results = [
            ScoredResult(content="Mock result 1", score=0.95, source="vector"),
            ScoredResult(content="Mock result 2", score=0.88, source="episodic"),
            ScoredResult(content="Mock result 3", score=0.82, source="semantic")
        ]
        
        processing_time_ms = int((time.time() - start_time) * 1000)
        
        return QueryResponse(
            query=request.query,
            response="Mock synthesized response",
            confidence=0.90,
            results=results[:request.top_k],
            explanation={"note": "Mock explanation"},
            processing_time_ms=processing_time_ms
        )
    
    def get_stats(self) -> Dict[str, Any]:
        return {
            "episodic_count": len(self.episodes),
            "vector_index_size": len(self.documents),
            "semantic_entity_count": 0,
            "semantic_relation_count": 0
        }

# Initialize backend (real or mock)
try:
    brain_ai = RealBrainAI() if BINDINGS_AVAILABLE else MockBrainAI()
except Exception as e:
    logger.error(f"Failed to initialize RealBrainAI: {e}")
    logger.info("Falling back to MockBrainAI")
    brain_ai = MockBrainAI()
    BINDINGS_AVAILABLE = False

# ==================== API Endpoints ====================

@app.get("/api/v1/health", response_model=HealthResponse)
async def health_check():
    """Health check endpoint"""
    return HealthResponse(
        status="healthy",
        version="1.0.0",
        timestamp=datetime.now().isoformat(),
        bindings_available=BINDINGS_AVAILABLE,
        components={
            "brain_ai": "operational" if BINDINGS_AVAILABLE else "mock",
            "api": "operational"
        }
    )

@app.get("/api/v1/stats", response_model=StatsResponse)
async def get_stats():
    """Get system statistics"""
    cpp_stats = brain_ai.get_stats()
    
    with stats_lock:
        avg_query_time = (
            stats["total_query_time_ms"] / stats["total_queries"]
            if stats["total_queries"] > 0 else 0
        )
        avg_doc_time = (
            stats["total_document_time_ms"] / stats["total_documents"]
            if stats["total_documents"] > 0 else 0
        )
        
        uptime = (datetime.now() - datetime.fromisoformat(SERVICE_INFO["started_at"])).total_seconds()
        
        return StatsResponse(
            total_documents=stats["total_documents"],
            successful_documents=stats["successful_documents"],
            total_queries=stats["total_queries"],
            successful_queries=stats["successful_queries"],
            episodic_count=cpp_stats["episodic_count"],
            vector_index_size=cpp_stats["vector_index_size"],
            semantic_entity_count=cpp_stats["semantic_entity_count"],
            semantic_relation_count=cpp_stats["semantic_relation_count"],
            uptime_seconds=uptime,
            avg_query_time_ms=avg_query_time,
            avg_document_time_ms=avg_doc_time
        )

@app.post("/api/v1/documents/process", response_model=DocumentProcessResponse)
async def process_document(request: DocumentProcessRequest):
    """Process a single document"""
    start_time = time.time()
    
    response = await brain_ai.process_document(request)
    
    processing_time = int((time.time() - start_time) * 1000)
    
    with stats_lock:
        stats["total_documents"] += 1
        stats["total_document_time_ms"] += processing_time
        if response.success:
            stats["successful_documents"] += 1
        else:
            stats["failed_documents"] += 1
    
    return response

@app.post("/api/v1/documents/batch", response_model=BatchDocumentResponse)
async def process_batch(request: BatchDocumentRequest):
    """Process multiple documents in batch"""
    start_time = time.time()
    
    results = []
    successful = 0
    failed = 0
    
    for doc_request in request.documents:
        response = await brain_ai.process_document(doc_request)
        results.append(response)
        
        if response.success:
            successful += 1
        else:
            failed += 1
    
    processing_time_ms = int((time.time() - start_time) * 1000)
    
    return BatchDocumentResponse(
        total=len(request.documents),
        successful=successful,
        failed=failed,
        results=results,
        processing_time_ms=processing_time_ms
    )

@app.post("/api/v1/query", response_model=QueryResponse)
async def process_query(request: QueryRequest):
    """Process a cognitive query"""
    start_time = time.time()
    
    response = await brain_ai.process_query(request)
    
    processing_time = int((time.time() - start_time) * 1000)
    
    with stats_lock:
        stats["total_queries"] += 1
        stats["total_query_time_ms"] += processing_time
        stats["successful_queries"] += 1
    
    return response

@app.post("/api/v1/index", response_model=IndexResponse)
async def index_document(request: IndexRequest):
    """Index a document directly"""
    try:
        if BINDINGS_AVAILABLE:
            brain_ai.handler.index_document(
                doc_id=request.doc_id,
                embedding=request.embedding,
                content=request.content,
                metadata=request.metadata
            )
        else:
            brain_ai.documents[request.doc_id] = {
                "content": request.content,
                "metadata": request.metadata
            }
        
        return IndexResponse(
            success=True,
            doc_id=request.doc_id,
            indexed=True
        )
    except Exception as e:
        return IndexResponse(
            success=False,
            doc_id=request.doc_id,
            indexed=False,
            error_message=str(e)
        )

@app.post("/api/v1/episodes", response_model=EpisodeResponse)
async def add_episode(request: EpisodeRequest):
    """Add an episode to memory"""
    try:
        if BINDINGS_AVAILABLE:
            brain_ai.handler.add_episode(
                content=request.content,
                embedding=request.embedding,
                importance=request.importance
            )
        else:
            brain_ai.episodes.append({
                "content": request.content,
                "timestamp": datetime.now().isoformat()
            })
        
        return EpisodeResponse(success=True)
    except Exception as e:
        return EpisodeResponse(success=False, error_message=str(e))

if __name__ == "__main__":
    import uvicorn
    
    logger.info("="*60)
    logger.info("Brain-AI REST Service Starting")
    logger.info(f"Bindings Available: {BINDINGS_AVAILABLE}")
    logger.info(f"Mode: {'Real C++ Integration' if BINDINGS_AVAILABLE else 'Mock Fallback'}")
    logger.info("="*60)
    
    uvicorn.run(app, host="0.0.0.0", port=5000, log_level="info")
