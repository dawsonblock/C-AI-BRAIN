#!/usr/bin/env python3
"""
Brain-AI REST Service
REST API wrapper for Brain-AI C++ library providing document processing and query capabilities
"""

from fastapi import APIRouter, Depends, FastAPI, HTTPException, Request, Security, status
from fastapi.responses import JSONResponse, PlainTextResponse
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import APIKeyHeader, HTTPAuthorizationCredentials, HTTPBearer
from pydantic import BaseModel
from typing import List, Optional, Dict, Any
import logging
import time
import asyncio
from datetime import datetime
import yaml
import os
from pathlib import Path
import threading
import secrets

from rate_limiter import enforce_concurrency, enforce_rate_limits, init_rate_limiters

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Load configuration from YAML
config_path = Path(__file__).parent / "config.yaml"
try:
    with open(config_path, 'r') as f:
        CONFIG = yaml.safe_load(f)
    logger.info(f"✅ Loaded configuration from {config_path}")
except Exception as e:
    logger.warning(f"⚠️  Failed to load config.yaml: {e}, using defaults")
    CONFIG = {}


def get_config(key_path: str, default=None):
    """Get config value from YAML or env var. key_path like 'cpp_backend.embedding_dim'"""
    env_key = key_path.upper().replace('.', '_')
    env_val = os.getenv(env_key)
    if env_val is not None:
        return env_val

    keys = key_path.split('.')
    val = CONFIG
    for k in keys:
        if isinstance(val, dict):
            val = val.get(k)
        else:
            return default
        if val is None:
            return default
    return val


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

API_KEY_HEADER = APIKeyHeader(name="X-API-Key", auto_error=False)
BEARER_SCHEME = HTTPBearer(auto_error=False)

MAX_UPLOAD_BYTES = 20 * 1024 * 1024  # 20 MB
MAX_TOKEN_COUNT = 256_000
DEFAULT_REQUEST_TIMEOUT = 60

# ==================== Middleware Configuration ====================

# CORS
cors_enabled = CONFIG.get('security', {}).get('cors_enabled', True)
if cors_enabled:
    cors_origins = CONFIG.get('security', {}).get('cors_origins', ["*"])
    app.add_middleware(
        CORSMiddleware,
        allow_origins=cors_origins,
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )
    logger.info(f"✅ CORS enabled for origins: {cors_origins}")

# Import middleware (after app creation)
try:
    from middleware import RequestTrackingMiddleware, get_request_tracker
    from metrics import metrics as metrics_collector

    # Request tracking
    app.add_middleware(RequestTrackingMiddleware)
    logger.info("✅ Request tracking middleware added")

    # Alias for convenience
    metrics = metrics_collector

    hist_samples_config = get_config('monitoring.histogram_max_samples')
    if hist_samples_config is not None:
        try:
            # Let metrics validate and coerce; do not pre-cast here
            metrics.set_histogram_max_samples(hist_samples_config)
            logger.info("✅ Histogram sample window set to %s", metrics.histogram_max_samples)
        except Exception as exc:
            logger.warning(
                "⚠️  Invalid monitoring.histogram_max_samples=%r (%s); using %s",
                hist_samples_config,
                exc,
                metrics.histogram_max_samples,
            )
except ImportError as e:
    logger.warning(f"⚠️  Middleware/metrics not available: {e}")
    metrics = None

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
stats_lock = threading.Lock()

# ==================== Initialize RAG++ Components ====================

API_KEY_ENV = get_config('security.api_key_env', 'BRAIN_AI_API_KEY')
REQUEST_TIMEOUT_SECONDS = int(get_config('security.request_timeout_seconds', DEFAULT_REQUEST_TIMEOUT))
RATE_LIMIT_RPS = int(get_config('security.rate_limit_rps', 10))
RATE_LIMIT_BURST = int(get_config('security.rate_limit_burst', 100))
MAX_CONCURRENT_REQUESTS = int(get_config('security.max_concurrent_requests', 8))


async def require_api_key(
    request: Request,
    header_key: Optional[str] = Security(API_KEY_HEADER),
    bearer_token: Optional[HTTPAuthorizationCredentials] = Security(BEARER_SCHEME),
) -> None:
    """Ensure every request presents the configured API key."""
    expected = os.getenv(API_KEY_ENV)
    if not expected:
        logger.error("❌ API key not configured")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Service misconfigured: API key missing",
        )

    provided = header_key or (bearer_token.credentials if bearer_token else None)
    if not provided:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Missing API key",
            headers={"WWW-Authenticate": 'Bearer, X-API-Key'},
        )

    if not secrets.compare_digest(provided, expected):
        client_ip = request.client.host if request.client else "unknown"
        logger.warning("Invalid API key attempt from %s", client_ip)
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Invalid API key",
        )

    request.state.api_key_validated = True


async def enforce_payload_limits(request: Request) -> None:
    """Bound request body size and approximate token count."""
    if request.method in {"GET", "HEAD", "OPTIONS"}:
        return

    body = await request.body()
    request.state.body = body  # allow downstream handlers to reuse the buffered body
    body_size = len(body)
    if body_size > MAX_UPLOAD_BYTES:
        logger.warning("Payload rejected (%d bytes)", body_size)
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail=f"Payload exceeds {MAX_UPLOAD_BYTES // (1024 * 1024)}MB limit",
        )

    if not body:
        return

    content_type = request.headers.get("content-type", "").lower()
    is_text_based = "application/json" in content_type or "text/" in content_type

    if not is_text_based:
        return # Skip token counting for binary data

    try:
        decoded = body.decode("utf-8")
    except UnicodeDecodeError:
        # Non-UTF8 payload for a content-type that should be text; skip token check.
        return

    token_count = len(decoded.split())
    if token_count > MAX_TOKEN_COUNT:
        logger.warning("Payload token count %d exceeds limit", token_count)
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail="Payload exceeds 256k token limit",
        )


async def run_with_timeout(coro):
    """Apply a hard timeout to backend operations."""
    try:
        return await asyncio.wait_for(coro, timeout=REQUEST_TIMEOUT_SECONDS)
    except asyncio.TimeoutError as exc:
        logger.error("Operation exceeded %ss timeout", REQUEST_TIMEOUT_SECONDS)
        raise HTTPException(
            status_code=status.HTTP_504_GATEWAY_TIMEOUT,
            detail=f"Operation exceeded {REQUEST_TIMEOUT_SECONDS}s timeout",
        ) from exc


init_rate_limiters(
    requests_per_minute=RATE_LIMIT_RPS * 60,
    burst_size=RATE_LIMIT_BURST,
    max_concurrent=MAX_CONCURRENT_REQUESTS,
)


@app.middleware("http")
async def attach_rate_limit_headers(request: Request, call_next):
    response = await call_next(request)
    info = getattr(request.state, "rate_limit_info", None)
    if info:
        response.headers.setdefault("X-RateLimit-Remaining", str(info.get("remaining", 0)))
        response.headers.setdefault("X-RateLimit-Reset", str(info.get("reset_seconds", 0)))
    return response


BrainAICognitiveHandler = None
BrainAIQueryConfig = None

try:
    from brain_ai import CognitiveHandler as BrainAICognitiveHandler, QueryConfig as BrainAIQueryConfig  # type: ignore
    logger.info("✅ brain_ai wheel detected")
except ImportError:
    logger.error("❌ Brain-AI wheel bindings not found; REST endpoints will use mock backend")
# Try to import C++ backend
USE_CPP_BACKEND = False
cognitive_handler = None

cpp_enabled = get_config('cpp_backend.enabled', True)
if cpp_enabled and BrainAICognitiveHandler:
    try:
        embedding_dim = int(get_config('cpp_backend.embedding_dim', 768))
        episodic_capacity = int(get_config('cpp_backend.episodic_capacity', 128))
        
        cognitive_handler = BrainAICognitiveHandler(
            episodic_capacity=episodic_capacity,
            embedding_dim=embedding_dim
        )
        USE_CPP_BACKEND = True
        logger.info(f"✅ C++ CognitiveHandler initialized (dim={embedding_dim}, capacity={episodic_capacity})")
    except Exception as e:
        logger.warning(f"⚠️  C++ backend unavailable, using mock: {e}")
        USE_CPP_BACKEND = False
elif cpp_enabled:
    logger.warning("⚠️  C++ backend enabled in config but bindings are missing")

# Initialize RAG++ components
from reranker import ReRanker
from chunker import SmartChunker
from facts_store import FactsStore
from llm_deepseek import DeepSeekClient
from multi_agent import MultiAgentOrchestrator
from sentence_transformers import SentenceTransformer
from persistence import PersistenceManager, AutoSaveMixin
from tools import TOOL_REGISTRY

# Embedding model for text → vector conversion
embedding_model = None
try:
    embedding_model_name = get_config('embeddings.model', 'all-mpnet-base-v2')
    embedding_model = SentenceTransformer(embedding_model_name)
    logger.info(f"✅ Embedding model initialized ({embedding_model_name})")
except Exception as e:
    logger.error(f"❌ Embedding model failed: {e}")

# Persistence manager
persistence_manager = PersistenceManager(
    data_dir=get_config('persistence.data_dir', 'data/persistence')
)

# Auto-save tracker
auto_saver = AutoSaveMixin(persistence_manager, save_interval_docs=10)

# Re-ranker for retrieval quality
reranker = None
if get_config('retrieval.reranker.enabled', True):
    try:
        reranker_model = get_config('retrieval.reranker.model', 'cross-encoder/ms-marco-MiniLM-L-6-v2')
        reranker = ReRanker(model_name=reranker_model)
        logger.info(f"✅ Re-ranker initialized ({reranker_model})")
    except Exception as e:
        logger.warning(f"⚠️  Re-ranker unavailable: {e}")

# Chunker for document processing
chunk_size = int(get_config('retrieval.chunking.chunk_size', 400))
overlap = int(get_config('retrieval.chunking.overlap', 50))
chunker = SmartChunker(chunk_size=chunk_size, overlap=overlap)
logger.info(f"✅ Smart chunker initialized (size={chunk_size}, overlap={overlap})")

# Facts store for high-confidence Q/A
facts_db_path = get_config('facts.db_path', 'data/facts.db')
facts_threshold = float(get_config('facts.confidence_threshold', 0.85))
facts_store = FactsStore(db_path=facts_db_path, confidence_threshold=facts_threshold)
logger.info(f"✅ Facts store initialized (threshold={facts_threshold})")

# DeepSeek LLM client
deepseek_client = None
api_key_env = get_config('llm.api_key_env', 'DEEPSEEK_API_KEY')
if os.getenv(api_key_env):
    try:
        base_url = get_config('llm.base_url', 'https://api.deepseek.com')
        max_retries = int(get_config('llm.retry.max_retries', 3))
        deepseek_client = DeepSeekClient(
            api_key=os.getenv(api_key_env),
            base_url=base_url,
            max_retries=max_retries
        )
        logger.info(f"✅ DeepSeek client initialized ({base_url})")
    except Exception as e:
        logger.error(f"❌ DeepSeek client failed: {e}")
else:
    logger.warning(f"⚠️  {api_key_env} not set - LLM features disabled")

# Multi-agent orchestrator with tools
multi_agent = None
if deepseek_client and get_config('multi_agent.enabled', True):
    n_solvers = int(get_config('multi_agent.solvers.n_solvers', 3))
    verification_threshold = float(get_config('multi_agent.verification.threshold', 0.85))
    multi_agent = MultiAgentOrchestrator(
        llm_client=deepseek_client,
        n_solvers=n_solvers,
        verification_threshold=verification_threshold,
        tools=TOOL_REGISTRY
    )
    logger.info(f"✅ Multi-agent orchestrator initialized (n_solvers={n_solvers}, tools={list(TOOL_REGISTRY.keys())})")

# Load persisted data if available
if cognitive_handler and USE_CPP_BACKEND:
    if persistence_manager.exists():
        logger.info("Loading persisted data...")
        results = persistence_manager.load_all(cognitive_handler)
        logger.info(f"Persistence loaded: {results}")
# ==================== Request/Response Models ====================

class OCRConfig(BaseModel):
    service_url: str = "http://localhost:8000"
    mode: str = "base"
    task: str = "ocr"
    max_tokens: int = 8192
    temperature: float = 0.0

class HistogramConfigRequest(BaseModel):
    sample_size: int


class HistogramConfigResponse(BaseModel):
    histogram_max_samples: int

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

api_router = APIRouter(
    prefix="/api/v1",
    dependencies=[
        Depends(require_api_key),
        Depends(enforce_rate_limits),
        Depends(enforce_concurrency),
        Depends(enforce_payload_limits),
    ],
)

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

@api_router.get("/health", response_model=HealthResponse)
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

@app.get("/metrics")
async def get_metrics(request: Request):
    """Prometheus metrics endpoint"""
    prometheus_enabled = get_config('monitoring.prometheus_enabled', False)
    if not prometheus_enabled:
        raise HTTPException(status_code=404, detail="Metrics disabled")
    
    if metrics is None:
        raise HTTPException(status_code=503, detail="Metrics subsystem unavailable")

    # Update gauges
    if cognitive_handler and USE_CPP_BACKEND:
        cpp_stats = cognitive_handler.get_stats()
        metrics.set_gauge("brain_ai_vector_index_size", cpp_stats.get("vector_index_size", 0))
        metrics.set_gauge("brain_ai_episodic_buffer_size", cpp_stats.get("episodic_buffer_size", 0))
        metrics.set_gauge("brain_ai_semantic_network_size", cpp_stats.get("semantic_network_size", 0))
    
    if facts_store:
        facts_stats = facts_store.get_stats()
        fact_count = facts_stats.get("total_facts")
        if fact_count is None:
            fact_count = facts_stats.get("fact_count", 0)
        metrics.set_gauge("brain_ai_facts_count", fact_count)
    
    # Request stats
    tracker = None
    if 'get_request_tracker' in globals():
        tracker = get_request_tracker()
    if tracker is None:
        tracker = getattr(request.app.state, "request_tracker", None)

    if tracker:
        tracker_stats = tracker.get_stats()
        metrics.set_gauge("brain_ai_total_requests", tracker_stats["total_requests"])
        metrics.set_gauge("brain_ai_total_errors", tracker_stats["total_errors"])
        metrics.set_gauge("brain_ai_error_rate", tracker_stats["error_rate"])
        metrics.set_gauge("brain_ai_avg_latency_ms", tracker_stats["avg_latency_ms"])
    
    return PlainTextResponse(content=metrics.export_prometheus(), media_type="text/plain")

@api_router.post("/monitoring/histogram_window", response_model=HistogramConfigResponse, dependencies=[Depends(require_api_key)])
async def configure_histogram_window(request: HistogramConfigRequest):
    """Adjust histogram rolling window for metrics latency tracking."""
    if metrics is None:
        raise HTTPException(status_code=503, detail="Metrics subsystem unavailable")

    try:
        metrics.set_histogram_max_samples(request.sample_size)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc

    logger.info("Histogram sample window adjusted to %s", metrics.histogram_max_samples)

    return HistogramConfigResponse(histogram_max_samples=metrics.histogram_max_samples)

@api_router.get("/monitoring/histogram_window", response_model=HistogramConfigResponse, dependencies=[Depends(require_api_key)])
async def get_histogram_window():
async def get_histogram_window():
    """Report current histogram sample window size."""
    if metrics is None:
        raise HTTPException(status_code=503, detail="Metrics subsystem unavailable")

    return HistogramConfigResponse(histogram_max_samples=metrics.histogram_max_samples)

@api_router.get("/stats", response_model=StatsResponse)
async def get_statistics():
    """Get service statistics"""
    uptime = (datetime.now() - datetime.fromisoformat(SERVICE_INFO["started_at"])).total_seconds()

    with stats_lock:
        snapshot = stats.copy()

    avg_query_time = (
        snapshot["total_query_time_ms"] / snapshot["total_queries"]
        if snapshot["total_queries"] > 0 else 0
    )

    avg_document_time = (
        snapshot["total_document_time_ms"] / snapshot["total_documents"]
        if snapshot["total_documents"] > 0 else 0
    )

    return StatsResponse(
        total_documents=snapshot["total_documents"],
        successful_documents=snapshot["successful_documents"],
        total_queries=snapshot["total_queries"],
        successful_queries=snapshot["successful_queries"],
        episodic_buffer_size=len(brain_ai.episodes),
        vector_index_size=len(brain_ai.vector_index),
        semantic_network_nodes=0,  # Mock value
        uptime_seconds=uptime,
        avg_query_time_ms=avg_query_time,
        avg_document_time_ms=avg_document_time
    )

@api_router.post("/documents/process", response_model=DocumentProcessResponse)
async def process_document(request: DocumentProcessRequest):
    """Process a single document"""
    logger.info(f"Processing document: {request.doc_id}")

    with stats_lock:
        stats["total_documents"] += 1

    result = await run_with_timeout(brain_ai.process_document(request))

    with stats_lock:
        if result.success:
            stats["successful_documents"] += 1
        else:
            stats["failed_documents"] += 1

        stats["total_document_time_ms"] += result.processing_time_ms

    return result

@api_router.post("/documents/batch", response_model=BatchDocumentResponse)
async def process_documents_batch(request: BatchDocumentRequest):
    """Process multiple documents in batch"""
    logger.info(f"Processing batch of {len(request.documents)} documents")
    
    start_time = time.time()
    
    tasks = []
    for doc_req in request.documents:
        if request.ocr_config:
            doc_req.ocr_config = request.ocr_config
        tasks.append(brain_ai.process_document(doc_req))
    
    results = await run_with_timeout(asyncio.gather(*tasks))
    
    total_time_ms = int((time.time() - start_time) * 1000)
    
    successful = sum(1 for r in results if r.success)
    failed = len(results) - successful
    
    return BatchDocumentResponse(
        total=len(results),
        successful=successful,
        failed=failed,
        results=results,
        total_time_ms=total_time_ms
    )

@api_router.post("/query", response_model=QueryResponse)
async def process_query(request: QueryRequest):
    """Process a cognitive query"""
    logger.info(f"Processing query: {request.query}")

    with stats_lock:
        stats["total_queries"] += 1

    try:
        result = await run_with_timeout(brain_ai.process_query(request))
    except Exception:
        with stats_lock:
            stats["failed_queries"] += 1
        raise

    with stats_lock:
        stats["successful_queries"] += 1
        stats["total_query_time_ms"] += result.processing_time_ms

    return result

@api_router.post("/search", response_model=SearchResponse)
async def search_similar(request: SearchRequest):
    """Search for similar documents"""
    logger.info(f"Searching with top_k={request.top_k}")
    
    result = await run_with_timeout(brain_ai.search_similar(request))
    
    return result

@api_router.post("/index", response_model=IndexResponse)
async def index_document(request: IndexRequest):
    """Index a document in the vector store (requires embedding)"""
    logger.info(f"Indexing document: {request.doc_id}")
    
    result = await run_with_timeout(brain_ai.index_document(request))
    
    return result

@api_router.post("/index_with_text")
async def index_with_text(request: dict):
    """Index a document by auto-generating embeddings from text"""
    doc_id = request.get("doc_id")
    text = request.get("text") or request.get("content")
    metadata = request.get("metadata", {})
    
    if not doc_id or not text:
        raise HTTPException(status_code=400, detail="doc_id and text required")
    
    if not embedding_model:
        raise HTTPException(status_code=503, detail="Embedding model not available")
    
    if not cognitive_handler or not USE_CPP_BACKEND:
        raise HTTPException(status_code=503, detail="C++ backend not available")
    
    try:
        start_time = time.time()
        
        # Generate embedding
        embedding = embedding_model.encode(text, convert_to_numpy=True).tolist()
        
        # Index in C++ backend
        success = cognitive_handler.index_document(
            doc_id=doc_id,
            embedding=embedding,
            content=text,
            metadata=metadata
        )
        
        processing_time = int((time.time() - start_time) * 1000)
        
        # Auto-save periodically
        auto_saver.maybe_save(cognitive_handler)
        
        return {
            "success": success,
            "doc_id": doc_id,
            "embedding_dim": len(embedding),
            "text_length": len(text),
            "processing_time_ms": processing_time
        }
            
    except Exception as e:
        logger.error(f"Indexing with text failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@api_router.post("/episodes", response_model=EpisodeResponse)
async def add_episode(request: EpisodeRequest):
    """Add an episode to episodic memory"""
    logger.info(f"Adding episode for query: {request.query}")
    
    result = await run_with_timeout(brain_ai.add_episode(request))
    
    return result

@api_router.get("/episodes/recent", response_model=EpisodesResponse)
async def get_recent_episodes(limit: int = 10):
    """Get recent episodes"""
    logger.info(f"Retrieving {limit} recent episodes")
    
    result = await run_with_timeout(brain_ai.get_recent_episodes(limit))
    
    return result

@api_router.post("/episodes/search", response_model=EpisodesResponse)
async def search_episodes(request: SearchRequest):
    """Search episodes by similarity"""
    logger.info(f"Searching episodes with top_k={request.top_k}")
    
    # For now, return recent episodes
    # In production, this would do actual vector search
    result = await run_with_timeout(brain_ai.get_recent_episodes(request.top_k))
    
    return result

# ==================== New RAG++ Endpoints ====================

@api_router.post("/answer")
async def answer_question(request: dict):
    """
    Answer question with optional multi-agent orchestration
    
    Request:
    {
        "question": str,
        "use_multi_agent": bool,  # Toggle for multi-agent mode
        "force_reasoning": bool,  # Force reasoning model
        "evidence_threshold": float  # Minimum confidence to answer
    }
    """
    question = request.get("question")
    if not question:
        raise HTTPException(status_code=400, detail="question required")
    
    use_multi_agent = request.get("use_multi_agent", False)
    force_reasoning = request.get("force_reasoning", False)
    evidence_threshold = request.get("evidence_threshold", 0.7)
    
    start_time = time.time()
    
    try:
        # Check facts cache first
        cached_fact = facts_store.lookup(question)
        if cached_fact:
            logger.info(f"✅ Cache hit for question")
            cached_fact["mode"] = "cached"
            cached_fact["processing_time_ms"] = int((time.time() - start_time) * 1000)
            return cached_fact
        
        # Generate query embedding and retrieve context
        if not embedding_model:
            raise HTTPException(status_code=503, detail="Embedding model not available")
        
        query_embedding_array = await run_with_timeout(
            asyncio.to_thread(
                embedding_model.encode,
                question,
                convert_to_numpy=True
            )
        )
        query_embedding = query_embedding_array.tolist()
        
        # Retrieve from vector store
        context_chunks = []
        context_text = ""
        
        if cognitive_handler and USE_CPP_BACKEND:
            # Build QueryConfig
            if not BrainAIQueryConfig:
                raise HTTPException(status_code=503, detail="C++ bindings missing QueryConfig")
            config = BrainAIQueryConfig()
            config.use_episodic = False
            config.use_semantic = False
            config.top_k_results = int(get_config('retrieval.vector_search.top_k', 50))
            
            # Query C++ backend
            response = await run_with_timeout(
                asyncio.to_thread(
                    cognitive_handler.process_query,
                    question,
                    query_embedding,
                    config
                )
            )
            
            # Extract context chunks from response
            # In real implementation, parse the response to get individual chunks
            context_text = response.response if hasattr(response, 'response') else ""
            
            # For re-ranking, we need structured chunks - parse from context_text
            # This is a simplified version; in production, the C++ backend should return structured data
            if context_text and context_text != "No relevant documents found.":
                # Split by paragraphs or sentences (simplified chunking)
                context_chunks = [chunk.strip() for chunk in context_text.split('\n\n') if chunk.strip()]
            
            # Apply re-ranking if enabled
            reranker_enabled = get_config('retrieval.reranker.enabled', False)
            if reranker_enabled and reranker and context_chunks:
                logger.info(f"🔄 Re-ranking {len(context_chunks)} chunks...")
                rerank_top_k = get_config('retrieval.reranker.rerank_top_k', 10)
                
                # Re-rank the chunks
                reranked_results = await run_with_timeout(
                    asyncio.to_thread(
                        reranker.rerank,
                        question,
                        context_chunks,
                        rerank_top_k
                    )
                )
                
                # Reorder chunks by score
                reranked_chunks = [context_chunks[idx] for idx, score in reranked_results]
                context_text = "\n\n".join(reranked_chunks)
                logger.info(f"✅ Re-ranked to top {len(reranked_results)} chunks")
            
            context = context_text if context_text else "No relevant context retrieved."
        else:
            context = "No vector store available - using empty context."
        
        # Generate answer
        if use_multi_agent and multi_agent:
            logger.info("🤖 Using multi-agent orchestration")
            result = await run_with_timeout(
                multi_agent.solve(
                    question,
                    context,
                    force_multi_agent=force_reasoning
                )
            )
            result["mode"] = "multi_agent"
        elif deepseek_client:
            logger.info("💬 Using single-agent with router")
            result = await run_with_timeout(
                asyncio.to_thread(
                    deepseek_client.generate_with_citations,
                    question,
                    context,
                    evidence_threshold
                )
            )
            result["mode"] = "single_agent"
        else:
            # Fallback: return context without LLM
            result = {
                "answer": f"LLM not available. Retrieved context: {context[:200]}...",
                "citations": [],
                "confidence": 0.5,
                "refuse": True
            }
            result["mode"] = "no_llm"
        
        # Store in facts if high confidence
        if result.get("confidence", 0) >= facts_store.confidence_threshold:
            await run_with_timeout(
                asyncio.to_thread(
                    facts_store.add_fact,
                    question,
                    result.get("answer", ""),
                    result.get("citations", []),
                    result["confidence"],
                    result["mode"]
                )
            )
        
        result["processing_time_ms"] = int((time.time() - start_time) * 1000)
        return result
        
    except Exception as e:
        logger.error(f"Answer generation failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@api_router.post("/chunk")
async def chunk_document(request: dict):
    """
    Chunk document with smart sentence-aware chunking
    
    Request:
    {
        "text": str,
        "doc_id": str,
        "chunk_size": int,
        "overlap": int
    }
    """
    text = request.get("text")
    doc_id = request.get("doc_id", "unknown")
    
    if not text:
        raise HTTPException(status_code=400, detail="text required")
    
    try:
        chunks = await run_with_timeout(
            asyncio.to_thread(
                chunker.chunk_document,
                text,
                doc_id,
                request.get("metadata")
            )
        )
        
        return {
            "doc_id": doc_id,
            "num_chunks": len(chunks),
            "chunks": chunks,
            "total_tokens": sum(c["token_count"] for c in chunks)
        }
    except Exception as e:
        logger.error(f"Chunking failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@api_router.get("/facts/stats")
async def get_facts_stats():
    """Get facts store statistics"""
    try:
        stats = facts_store.get_stats()
        return stats
    except Exception as e:
        logger.error(f"Facts stats failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@api_router.get("/system/status")
async def get_system_status():
    """Get comprehensive system status"""
    status = {
        "service": "brain-ai-rest-service",
        "version": "1.0.0",
        "timestamp": datetime.now().isoformat(),
        "components": {
            "cpp_backend": USE_CPP_BACKEND,
            "embedding_model": embedding_model is not None,
            "reranker": reranker is not None,
            "facts_store": True,
            "deepseek_llm": deepseek_client is not None,
            "multi_agent": multi_agent is not None,
            "persistence": True,
            "tools": list(TOOL_REGISTRY.keys())
        }
    }
    
    if cognitive_handler and USE_CPP_BACKEND:
        status["cpp_stats"] = cognitive_handler.get_stats()
    
    status["facts_stats"] = facts_store.get_stats()
    status["persistence_info"] = persistence_manager.get_info()
    
    return status

@api_router.post("/persistence/save")
async def save_persistence():
    """Manually trigger persistence save"""
    if not cognitive_handler:
        raise HTTPException(status_code=503, detail="C++ backend not available")
    
    try:
        results = await run_with_timeout(
            asyncio.to_thread(
                persistence_manager.save_all,
                cognitive_handler,
                facts_store
            )
        )
        return {
            "success": True,
            "results": results,
            "info": persistence_manager.get_info()
        }
    except Exception as e:
        logger.error(f"Save failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@api_router.post("/persistence/load")
async def load_persistence():
    """Manually trigger persistence load"""
    if not cognitive_handler:
        raise HTTPException(status_code=503, detail="C++ backend not available")
    
    try:
        results = await run_with_timeout(
            asyncio.to_thread(
                persistence_manager.load_all,
                cognitive_handler
            )
        )
        return {
            "success": True,
            "results": results,
            "info": persistence_manager.get_info()
        }
    except Exception as e:
        logger.error(f"Load failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


app.include_router(api_router)


if __name__ == "__main__":
    import uvicorn
    
    logger.info("🚀 Starting Brain-AI REST Service (RAG++)...")
    logger.info(f"Service info: {SERVICE_INFO}")
    logger.info(f"C++ Backend: {'✅ Enabled' if USE_CPP_BACKEND else '❌ Disabled'}")
    logger.info(f"Multi-Agent: {'✅ Enabled' if multi_agent else '❌ Disabled'}")
    
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=5001,
        log_level="info",
        access_log=True
    )
