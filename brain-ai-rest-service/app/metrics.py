"""Prometheus metrics definitions with bounded label cardinality."""

from __future__ import annotations

from prometheus_client import Counter, Gauge, Histogram, CollectorRegistry, generate_latest


REGISTRY = CollectorRegistry()

# HTTP surface metrics
HTTP_REQUESTS = Counter(
    "http_requests_total",
    "Total HTTP requests",
    labelnames=("route", "status"),
    registry=REGISTRY,
)

REQUEST_LATENCY = Histogram(
    "request_latency_seconds",
    "Latency per HTTP route",
    labelnames=("route",),
    registry=REGISTRY,
)

# Domain metrics
INDEXED_DOCUMENTS = Counter(
    "index_docs_total",
    "Number of documents indexed",
    registry=REGISTRY,
)

QUERY_LATENCY = Histogram(
    "query_latency_seconds",
    "Latency of query operations",
    registry=REGISTRY,
)

OCR_CALLS = Counter(
    "ocr_calls_total",
    "OCR requests made",
    registry=REGISTRY,
)

ERROR_COUNT = Counter(
    "errors_total",
    "Count of errors by component",
    labelnames=("component",),
    registry=REGISTRY,
)

INDEX_SIZE = Gauge(
    "index_size_total",
    "Documents stored in index",
    registry=REGISTRY,
)


def collect_metrics() -> bytes:
    """Render metrics in Prometheus exposition format."""

    return generate_latest(REGISTRY)


__all__ = [
    "HTTP_REQUESTS",
    "REQUEST_LATENCY",
    "INDEXED_DOCUMENTS",
    "QUERY_LATENCY",
    "OCR_CALLS",
    "ERROR_COUNT",
    "INDEX_SIZE",
    "collect_metrics",
    "REGISTRY",
]
