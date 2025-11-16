"""Main FastAPI application with production security hardening."""

import time
import logging
from contextlib import asynccontextmanager
from typing import Any, Dict, List, Optional

from fastapi import FastAPI, Depends, HTTPException, status
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from slowapi.errors import RateLimitExceeded

from .config import settings
from .logging_setup import setup_logging
from .middleware import (
    RequestIdMiddleware,
    RequestLoggingMiddleware,
    BodySizeLimitMiddleware,
    SecurityHeadersMiddleware,
    limiter,
    rate_limit_exceeded_handler,
)
from .dependencies import require_api_key
from .models import (
    HealthResponse,
    IndexRequest,
    IndexResponse,
    QueryRequest,
    QueryResponse,
    ChatRequest,
    ChatResponse,
    CalculateRequest,
    CalculateResponse,
    ErrorResponse,
)
from .security import safe_eval, SafeEvaluationError
from .database import initialize_database, get_db_pool
from .metrics import generate_metrics, record_document_indexed, record_query, record_calculation
from .llm_router import llm_chat, LLMRouterError

# Setup logging
setup_logging(log_level=settings.log_level, json_logs=settings.log_json)
logger = logging.getLogger(__name__)


def _extract_llm_answer(payload: Dict[str, Any]) -> Optional[str]:
    """Best-effort extraction of answer content from an LLM payload."""
    try:
        choices = payload.get("choices")
        if not choices:
            return None
        first = choices[0] or {}
        message = first.get("message") or {}
        content = message.get("content")
        if isinstance(content, str):
            return content
    except Exception:
        return None
    return None


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan events."""
    # Startup
    logger.info(
        f"Starting {settings.service_name} v{settings.version}",
        extra={
            "environment": settings.environment,
            "log_level": settings.log_level,
        }
    )
    
    # Initialize database
    try:
        initialize_database(settings.db_path)
        logger.info("Database initialized successfully")
    except Exception as e:
        logger.error(f"Failed to initialize database: {e}", exc_info=True)
        raise
    
    yield
    
    # Shutdown
    logger.info("Shutting down application")
    try:
        pool = get_db_pool()
        pool.close_all()
        logger.info("Database connections closed")
    except Exception as e:
        logger.error(f"Error during shutdown: {e}", exc_info=True)


# Create FastAPI app
app = FastAPI(
    title="Brain AI REST Service",
    description="Production-ready AI service with embeddings, document indexing, and secure computation",
    version=settings.version,
    lifespan=lifespan,
    docs_url="/docs" if settings.environment != "production" else None,
    redoc_url="/redoc" if settings.environment != "production" else None,
)

# Add rate limiter state
app.state.limiter = limiter

# Add middlewares (order matters - first added = outermost)
app.add_middleware(SecurityHeadersMiddleware)
app.add_middleware(RequestLoggingMiddleware)
app.add_middleware(RequestIdMiddleware)
app.add_middleware(BodySizeLimitMiddleware, max_size=settings.max_request_size)

# CORS - only if origins are explicitly configured
if settings.cors_origins:
    logger.info(f"CORS enabled for origins: {settings.cors_origins}")
    app.add_middleware(
        CORSMiddleware,
        allow_origins=settings.cors_origins,
        allow_credentials=settings.cors_credentials,
        allow_methods=settings.cors_methods,
        allow_headers=settings.cors_headers,
    )
else:
    logger.info("CORS disabled - no origins configured")

# Exception handlers
app.add_exception_handler(RateLimitExceeded, rate_limit_exceeded_handler)


@app.exception_handler(Exception)
async def global_exception_handler(request, exc: Exception) -> JSONResponse:
    """Handle unexpected exceptions."""
    logger.error(
        f"Unhandled exception: {type(exc).__name__}",
        extra={
            "error": str(exc),
            "path": request.url.path,
        },
        exc_info=True
    )
    return JSONResponse(
        status_code=500,
        content=ErrorResponse(
            error="Internal server error",
            request_id=getattr(request.state, "request_id", "unknown")
        ).model_dump()
    )


# Health and readiness endpoints
@app.get("/health", response_model=HealthResponse, tags=["Health"])
@app.get("/healthz", response_model=HealthResponse, tags=["Health"])
async def health_check() -> HealthResponse:
    """
    Health check endpoint.
    
    Returns basic service health without dependencies.
    """
    return HealthResponse(
        status="healthy",
        version=settings.version,
        service=settings.service_name,
        timestamp=time.time()
    )


@app.get("/ready", response_model=HealthResponse, tags=["Health"])
@app.get("/readyz", response_model=HealthResponse, tags=["Health"])
async def readiness_check() -> HealthResponse:
    """
    Readiness check endpoint.
    
    Verifies all dependencies are available.
    """
    checks = {}
    overall_status = "healthy"
    
    # Check database
    try:
        pool = get_db_pool()
        conn = pool.get_connection()
        cursor = conn.cursor()
        cursor.execute("SELECT 1")
        cursor.fetchone()
        pool.release_connection(conn)
        checks["database"] = "healthy"
    except Exception as e:
        logger.error(f"Database health check failed: {e}")
        checks["database"] = f"unhealthy: {type(e).__name__}"
        overall_status = "unhealthy"
    
    return HealthResponse(
        status=overall_status,
        version=settings.version,
        service=settings.service_name,
        timestamp=time.time(),
        checks=checks
    )


# Metrics endpoint
@app.get("/metrics", tags=["Observability"])
async def metrics():
    """
    Prometheus metrics endpoint.
    
    Returns metrics in Prometheus text format.
    """
    if not settings.metrics_enabled:
        raise HTTPException(
            status_code=404,
            detail="Metrics disabled"
        )
    return generate_metrics()


# API endpoints (require authentication)
@app.post(
    "/index",
    response_model=IndexResponse,
    tags=["Documents"],
    dependencies=[Depends(require_api_key)]
)
@limiter.limit("50/minute")
async def index_document(request: Any, payload: IndexRequest) -> IndexResponse:
    """
    Index a document with embeddings.
    
    Requires API key authentication.
    """
    logger.info(f"Indexing document: {payload.document_id}")
    
    try:
        # TODO: Implement actual indexing with embeddings
        # This is a placeholder for the real implementation
        record_document_indexed()
        
        return IndexResponse(
            success=True,
            document_id=payload.document_id,
            message="Document indexed successfully"
        )
    except Exception as e:
        logger.error(f"Failed to index document: {e}", exc_info=True)
        raise HTTPException(
            status_code=500,
            detail="Failed to index document"
        )


@app.post(
    "/query",
    response_model=QueryResponse,
    tags=["Search"],
    dependencies=[Depends(require_api_key)]
)
@limiter.limit("100/minute")
async def query_documents(request: Any, payload: QueryRequest) -> QueryResponse:
    """
    Query indexed documents.
    
    Requires API key authentication.
    """
    logger.info(f"Processing query: {payload.query[:50]}...")
    start_time = time.time()
    
    try:
        # TODO: Implement actual query with embeddings
        # This is a placeholder for the real implementation
        record_query()
        
        query_time_ms = (time.time() - start_time) * 1000

        answer: Optional[str] = None
        # Only attempt LLM answer generation outside of test environment
        if settings.environment != "test":
            try:
                messages: List[Dict[str, str]] = [
                    {
                        "role": "system",
                        "content": "You are a helpful assistant that answers user queries.",
                    },
                    {
                        "role": "user",
                        "content": payload.query,
                    },
                ]
                raw = await llm_chat(messages)
                answer = _extract_llm_answer(raw)
            except LLMRouterError as exc:
                logger.warning("LLM answer generation failed", extra={"error": str(exc)})
        
        return QueryResponse(
            success=True,
            results=[],
            query_time_ms=query_time_ms,
            answer=answer,
        )
    except Exception as e:
        logger.error(f"Failed to process query: {e}", exc_info=True)
        raise HTTPException(
            status_code=500,
            detail="Failed to process query"
        )


@app.post(
    "/calculate",
    response_model=CalculateResponse,
    tags=["Compute"],
    dependencies=[Depends(require_api_key)]
)
@limiter.limit("200/minute")
async def calculate(request: Any, payload: CalculateRequest) -> CalculateResponse:
    """
    Safely evaluate mathematical expressions.
    
    Only allows basic math operations and whitelisted functions.
    No code execution, imports, or variable assignment.
    
    Requires API key authentication.
    """
    logger.info(f"Evaluating expression: {payload.expression}")
    
    try:
        result = safe_eval(payload.expression)
        record_calculation(success=True)
        
        return CalculateResponse(
            success=True,
            result=result,
            expression=payload.expression
        )
    except SafeEvaluationError as e:
        logger.warning(f"Safe evaluation failed: {e}")
        record_calculation(success=False)
        
        return CalculateResponse(
            success=False,
            result=None,
            expression=payload.expression,
            error=str(e)
        )
    except Exception as e:
        logger.error(f"Unexpected error in calculate: {e}", exc_info=True)
        record_calculation(success=False)
        
        raise HTTPException(
            status_code=500,
            detail="Calculation failed"
        )


@app.post(
    "/chat",
    response_model=ChatResponse,
    tags=["LLM"],
    dependencies=[Depends(require_api_key)],
)
@limiter.limit("100/minute")
async def chat(request: Any, payload: ChatRequest) -> ChatResponse:
    provider = payload.provider or settings.llm_provider
    logger.info("Handling chat request", extra={"provider": provider})

    try:
        raw = await llm_chat(
            [m.model_dump() for m in payload.messages],
            provider=payload.provider,
        )
        answer = _extract_llm_answer(raw)
        return ChatResponse(
            success=True,
            provider=provider,
            answer=answer,
            raw=raw,
        )
    except LLMRouterError as exc:
        logger.warning("LLM chat failed", extra={"error": str(exc)})
        return ChatResponse(
            success=False,
            provider=provider,
            answer=None,
            raw=None,
            error=str(exc),
        )


# Root endpoint
@app.get("/", tags=["Info"])
async def root():
    """API information."""
    return {
        "service": settings.service_name,
        "version": settings.version,
        "status": "operational",
        "docs": "/docs" if settings.environment != "production" else "disabled in production"
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "app.app:app",
        host="0.0.0.0",
        port=8000,
        log_config=None,  # Use our custom logging
        access_log=False,  # We handle this in middleware
    )
