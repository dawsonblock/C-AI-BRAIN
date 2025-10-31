#!/usr/bin/env python3
"""
Brain-AI REST Service
REST API wrapper for Brain-AI C++ library providing document processing and query capabilities
"""

from fastapi import FastAPI, HTTPException, File, UploadFile
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from typing import List, Optional, Dict, Any
import logging
import time
import asyncio
from datetime import datetime

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

app = FastAPI(
    title="Brain-AI REST Service",
    description="REST API for Brain-AI document processing and cognitive query system",
    version="1.0.0"
)

# Service metadata
SERVICE_INFO = {
    "name": "brain-ai-rest-service",
    "version": "1.0.0",
    "started_at": datetime.now().isoformat()
}

# Statistics tracking
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
    image_data: Optional[str] = None  # Base64 encoded
    mime_type: Optional[str] = "application/pdf"
    ocr_config: Optional[OCRConfig] = OCRConfig()
    create_memory: bool = True
    index_document: bool = True

class DocumentProcessResponse(BaseModel):
    success: bool
    doc_id: str
    extracted_text: str = ""
    validated_text: str = ""
    ocr_confidence: float = 0.0
    validation_confidence: float = 0.0
    indexed: bool = False
    processing_time_ms: int = 0
    error_message: str = ""

class BatchDocumentRequest(BaseModel):
    documents: List[DocumentProcessRequest]
    ocr_config: Optional[OCRConfig] = OCRConfig()

class BatchDocumentResponse(BaseModel):
    total: int
    successful: int
    failed: int
    results: List[DocumentProcessResponse]
    total_time_ms: int

class QueryRequest(BaseModel):
    query: str
    query_embedding: List[float]
    top_k: int = 10
    use_episodic: bool = True
    use_semantic: bool = True
    check_hallucination: bool = True
    generate_explanation: bool = True

class ScoredResult(BaseModel):
    content: str
    score: float
    source: str

class QueryResponse(BaseModel):
    query: str
    response: str
    confidence: float
    results: List[ScoredResult]
    explanation: Dict[str, Any]
    processing_time_ms: int

class SearchRequest(BaseModel):
    query_embedding: List[float]
    top_k: int = 10
    similarity_threshold: float = 0.0

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
    query: str
    response: str
    query_embedding: List[float]
    metadata: Dict[str, Any] = {}

class EpisodeResponse(BaseModel):
    success: bool
    episode_id: str
    error_message: str = ""

class Episode(BaseModel):
    query: str
    response: str
    timestamp: str
    relevance: float = 1.0

class EpisodesResponse(BaseModel):
    episodes: List[Episode]
    total: int

class HealthResponse(BaseModel):
    status: str
    version: str
    timestamp: str
    components: Dict[str, str]

class StatsResponse(BaseModel):
    total_documents: int
    successful_documents: int
    total_queries: int
    successful_queries: int
    episodic_buffer_size: int
    vector_index_size: int
    semantic_network_nodes: int
    uptime_seconds: float
    avg_query_time_ms: float
    avg_document_time_ms: float

# ==================== Mock Brain-AI Backend ====================
# This simulates the C++ library until pybind11 bindings are created

class MockBrainAI:
    """Mock implementation of Brain-AI functionality"""
    
    def __init__(self):
        self.documents = {}
        self.episodes = []
        self.vector_index = {}
        logger.info("MockBrainAI initialized")
    
    async def process_document(self, request: DocumentProcessRequest) -> DocumentProcessResponse:
        """Mock document processing"""
        start_time = time.time()
        
        try:
            # Simulate OCR call
            await asyncio.sleep(0.3)  # Simulate OCR processing time
            
            extracted_text = f"Extracted text from {request.doc_id}. "
            extracted_text += "This is mock text that would normally come from the OCR service. "
            extracted_text += "In production, this would be actual document content."
            
            # Simulate text validation
            validated_text = extracted_text.strip()
            
            # Simulate indexing
            if request.index_document:
                self.documents[request.doc_id] = {
                    "text": validated_text,
                    "indexed_at": datetime.now().isoformat()
                }
            
            processing_time_ms = int((time.time() - start_time) * 1000)
            
            return DocumentProcessResponse(
                success=True,
                doc_id=request.doc_id,
                extracted_text=extracted_text,
                validated_text=validated_text,
                ocr_confidence=0.92,
                validation_confidence=0.88,
                indexed=request.index_document,
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
        """Mock query processing"""
        start_time = time.time()
        
        try:
            # Simulate query processing
            await asyncio.sleep(0.2)
            
            # Mock results
            results = [
                ScoredResult(
                    content="Result 1: Relevant information about the query",
                    score=0.95,
                    source="vector"
                ),
                ScoredResult(
                    content="Result 2: Additional context from episodic memory",
                    score=0.88,
                    source="episodic"
                ),
                ScoredResult(
                    content="Result 3: Related semantic concepts",
                    score=0.82,
                    source="semantic"
                )
            ]
            
            response_text = f"Based on the query '{request.query}', here is a synthesized response. "
            response_text += "This combines information from vector search, episodic memory, and semantic network."
            
            processing_time_ms = int((time.time() - start_time) * 1000)
            
            return QueryResponse(
                query=request.query,
                response=response_text,
                confidence=0.90,
                results=results[:request.top_k],
                explanation={
                    "reasoning": "Used hybrid fusion of vector, episodic, and semantic sources",
                    "confidence": 0.88,
                    "sources_used": ["vector", "episodic", "semantic"]
                },
                processing_time_ms=processing_time_ms
            )
            
        except Exception as e:
            logger.error(f"Error processing query: {e}")
            raise HTTPException(status_code=500, detail=str(e))
    
    async def search_similar(self, request: SearchRequest) -> SearchResponse:
        """Mock vector search"""
        await asyncio.sleep(0.1)
        
        # Mock search results
        results = []
        for i, (doc_id, doc_data) in enumerate(list(self.documents.items())[:request.top_k]):
            results.append(SearchResult(
                doc_id=doc_id,
                content=doc_data["text"][:200] + "...",
                similarity=0.95 - (i * 0.05),
                metadata={"indexed_at": doc_data["indexed_at"]}
            ))
        
        return SearchResponse(
            results=results,
            total_results=len(results)
        )
    
    async def index_document(self, request: IndexRequest) -> IndexResponse:
        """Mock document indexing"""
        try:
            self.vector_index[request.doc_id] = {
                "embedding": request.embedding,
                "content": request.content,
                "metadata": request.metadata,
                "indexed_at": datetime.now().isoformat()
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
    
    async def add_episode(self, request: EpisodeRequest) -> EpisodeResponse:
        """Mock episode addition"""
        try:
            episode_id = f"ep_{len(self.episodes) + 1}"
            self.episodes.append({
                "id": episode_id,
                "query": request.query,
                "response": request.response,
                "timestamp": datetime.now().isoformat(),
                "metadata": request.metadata
            })
            
            return EpisodeResponse(
                success=True,
                episode_id=episode_id
            )
        except Exception as e:
            return EpisodeResponse(
                success=False,
                episode_id="",
                error_message=str(e)
            )
    
    async def get_recent_episodes(self, limit: int = 10) -> EpisodesResponse:
        """Mock recent episodes retrieval"""
        recent = self.episodes[-limit:]
        
        episodes = [
            Episode(
                query=ep["query"],
                response=ep["response"],
                timestamp=ep["timestamp"],
                relevance=1.0
            )
            for ep in recent
        ]
        
        return EpisodesResponse(
            episodes=episodes,
            total=len(episodes)
        )

# Initialize mock backend
brain_ai = MockBrainAI()

# ==================== API Endpoints ====================

@app.get("/")
async def root():
    """Root endpoint"""
    return {
        "service": SERVICE_INFO["name"],
        "version": SERVICE_INFO["version"],
        "status": "healthy",
        "endpoints": {
            "health": "/api/v1/health",
            "stats": "/api/v1/stats",
            "docs": "/docs",
            "api": "/api/v1"
        }
    }

@app.get("/api/v1/health", response_model=HealthResponse)
async def health_check():
    """Health check endpoint"""
    return HealthResponse(
        status="healthy",
        version=SERVICE_INFO["version"],
        timestamp=datetime.now().isoformat(),
        components={
            "brain_ai": "healthy",
            "ocr_service": "connected",
            "vector_store": "healthy",
            "episodic_buffer": "healthy"
        }
    )

@app.get("/api/v1/stats", response_model=StatsResponse)
async def get_statistics():
    """Get service statistics"""
    uptime = (datetime.now() - datetime.fromisoformat(SERVICE_INFO["started_at"])).total_seconds()
    
    avg_query_time = (
        stats["total_query_time_ms"] / stats["total_queries"]
        if stats["total_queries"] > 0 else 0
    )
    
    avg_document_time = (
        stats["total_document_time_ms"] / stats["total_documents"]
        if stats["total_documents"] > 0 else 0
    )
    
    return StatsResponse(
        total_documents=stats["total_documents"],
        successful_documents=stats["successful_documents"],
        total_queries=stats["total_queries"],
        successful_queries=stats["successful_queries"],
        episodic_buffer_size=len(brain_ai.episodes),
        vector_index_size=len(brain_ai.vector_index),
        semantic_network_nodes=0,  # Mock value
        uptime_seconds=uptime,
        avg_query_time_ms=avg_query_time,
        avg_document_time_ms=avg_document_time
    )

@app.post("/api/v1/documents/process", response_model=DocumentProcessResponse)
async def process_document(request: DocumentProcessRequest):
    """Process a single document"""
    logger.info(f"Processing document: {request.doc_id}")
    
    stats["total_documents"] += 1
    
    result = await brain_ai.process_document(request)
    
    if result.success:
        stats["successful_documents"] += 1
    else:
        stats["failed_documents"] += 1
    
    stats["total_document_time_ms"] += result.processing_time_ms
    
    return result

@app.post("/api/v1/documents/batch", response_model=BatchDocumentResponse)
async def process_documents_batch(request: BatchDocumentRequest):
    """Process multiple documents in batch"""
    logger.info(f"Processing batch of {len(request.documents)} documents")
    
    start_time = time.time()
    start_time = time.time()
    
    tasks = []
    for doc_req in request.documents:
        if request.ocr_config:
            doc_req.ocr_config = request.ocr_config
        tasks.append(brain_ai.process_document(doc_req))
    
    results = await asyncio.gather(*tasks)
    
    
    successful = sum(1 for r in results if r.success)
    failed = len(results) - successful
    
    return BatchDocumentResponse(
        total=len(results),
        successful=successful,
        failed=failed,
        results=results,
        total_time_ms=total_time_ms
    )

@app.post("/api/v1/query", response_model=QueryResponse)
async def process_query(request: QueryRequest):
    """Process a cognitive query"""
    logger.info(f"Processing query: {request.query}")
    
    stats["total_queries"] += 1
    
    result = await brain_ai.process_query(request)
    
    stats["successful_queries"] += 1
    stats["total_query_time_ms"] += result.processing_time_ms
    
    return result

@app.post("/api/v1/search", response_model=SearchResponse)
async def search_similar(request: SearchRequest):
    """Search for similar documents"""
    logger.info(f"Searching with top_k={request.top_k}")
    
    result = await brain_ai.search_similar(request)
    
    return result

@app.post("/api/v1/index", response_model=IndexResponse)
async def index_document(request: IndexRequest):
    """Index a document in the vector store"""
    logger.info(f"Indexing document: {request.doc_id}")
    
    result = await brain_ai.index_document(request)
    
    return result

@app.post("/api/v1/episodes", response_model=EpisodeResponse)
async def add_episode(request: EpisodeRequest):
    """Add an episode to episodic memory"""
    logger.info(f"Adding episode for query: {request.query}")
    
    result = await brain_ai.add_episode(request)
    
    return result

@app.get("/api/v1/episodes/recent", response_model=EpisodesResponse)
async def get_recent_episodes(limit: int = 10):
    """Get recent episodes"""
    logger.info(f"Retrieving {limit} recent episodes")
    
    result = await brain_ai.get_recent_episodes(limit)
    
    return result

@app.post("/api/v1/episodes/search", response_model=EpisodesResponse)
async def search_episodes(request: SearchRequest):
    """Search episodes by similarity"""
    logger.info(f"Searching episodes with top_k={request.top_k}")
    
    # For now, return recent episodes
    # In production, this would do actual vector search
    result = await brain_ai.get_recent_episodes(request.top_k)
    
    return result

if __name__ == "__main__":
    import uvicorn
    
    logger.info("Starting Brain-AI REST Service...")
    logger.info(f"Service info: {SERVICE_INFO}")
    
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=5000,
        log_level="info",
        access_log=True
    )
