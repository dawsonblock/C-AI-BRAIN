"""Prometheus metrics collection."""

from prometheus_client import Counter, Histogram, Gauge, generate_latest, CONTENT_TYPE_LATEST
from starlette.responses import Response
import logging

logger = logging.getLogger(__name__)

# Request metrics
request_total = Counter(
    "brain_ai_requests_total",
    "Total number of requests",
    ["method", "endpoint", "status"]
)

request_duration = Histogram(
    "brain_ai_request_duration_seconds",
    "Request duration in seconds",
    ["method", "endpoint"]
)

# Business metrics
documents_indexed = Counter(
    "brain_ai_documents_indexed_total",
    "Total number of documents indexed"
)

queries_processed = Counter(
    "brain_ai_queries_processed_total",
    "Total number of queries processed"
)

calculations_performed = Counter(
    "brain_ai_calculations_total",
    "Total number of safe calculations performed",
    ["status"]
)

embedding_latency = Histogram(
    "brain_ai_embedding_latency_seconds",
    "Time to generate embeddings",
    ["backend"]
)

# System metrics
active_connections = Gauge(
    "brain_ai_active_connections",
    "Number of active database connections"
)

db_operations = Counter(
    "brain_ai_db_operations_total",
    "Total database operations",
    ["operation", "status"]
)


def generate_metrics() -> Response:
    """
    Generate Prometheus metrics response.
    
    Returns:
        Response with metrics in Prometheus text format
    """
    metrics = generate_latest()
    return Response(
        content=metrics,
        media_type=CONTENT_TYPE_LATEST
    )


def record_request(method: str, endpoint: str, status: int, duration: float) -> None:
    """
    Record request metrics.
    
    Args:
        method: HTTP method
        endpoint: API endpoint
        status: Response status code
        duration: Request duration in seconds
    """
    request_total.labels(
        method=method,
        endpoint=endpoint,
        status=status
    ).inc()
    
    request_duration.labels(
        method=method,
        endpoint=endpoint
    ).observe(duration)


def record_document_indexed() -> None:
    """Record document indexing."""
    documents_indexed.inc()


def record_query() -> None:
    """Record query processing."""
    queries_processed.inc()


def record_calculation(success: bool) -> None:
    """
    Record calculation attempt.
    
    Args:
        success: Whether calculation succeeded
    """
    status = "success" if success else "failure"
    calculations_performed.labels(status=status).inc()


def record_embedding_time(backend: str, duration: float) -> None:
    """
    Record embedding generation time.
    
    Args:
        backend: Embedding backend used
        duration: Duration in seconds
    """
    embedding_latency.labels(backend=backend).observe(duration)


def record_db_operation(operation: str, success: bool) -> None:
    """
    Record database operation.
    
    Args:
        operation: Type of operation (select, insert, update, delete)
        success: Whether operation succeeded
    """
    status = "success" if success else "failure"
    db_operations.labels(
        operation=operation,
        status=status
    ).inc()
