"""
Middleware for Brain-AI REST API
- API key authentication
- Request tracking
- Rate limiting integration
"""
import os
import time
import logging
import threading
from typing import Callable

from fastapi import Request, HTTPException, status
from fastapi.responses import JSONResponse
from starlette.middleware.base import BaseHTTPMiddleware

logger = logging.getLogger(__name__)


try:
    from metrics import metrics as metrics_collector
except ImportError:  # pragma: no cover - metrics optional during unit tests
    metrics_collector = None


class APIKeyMiddleware(BaseHTTPMiddleware):
    """Middleware to enforce API key authentication"""
    
    def __init__(self, app, api_key_required: bool = False, api_key_env: str = "BRAIN_AI_API_KEY"):
        super().__init__(app)
        self.api_key_required = api_key_required
        self.api_key = os.getenv(api_key_env) if api_key_required else None
        self.exempt_paths = {"/health", "/metrics", "/docs", "/redoc", "/openapi.json"}
        
        if api_key_required and not self.api_key:
            logger.warning(f"⚠️  API key required but {api_key_env} not set!")
        elif api_key_required:
            logger.info(f"✅ API key authentication enabled")
    
    async def dispatch(self, request: Request, call_next: Callable):
        # Skip auth for exempt paths
        if request.url.path in self.exempt_paths:
            return await call_next(request)
        
        # Check API key if required
        if self.api_key_required and self.api_key:
            provided_key = request.headers.get("X-API-Key") or request.headers.get("Authorization", "").replace("Bearer ", "")
            
            if not provided_key:
                return JSONResponse(
                    status_code=status.HTTP_401_UNAUTHORIZED,
                    content={"detail": "API key required. Provide via X-API-Key header or Authorization: Bearer <key>"}
                )
            
            if provided_key != self.api_key:
                logger.warning(f"Invalid API key attempt from {request.client.host}")
                return JSONResponse(
                    status_code=status.HTTP_403_FORBIDDEN,
                    content={"detail": "Invalid API key"}
                )
        
        return await call_next(request)


_tracker_instance_lock = threading.Lock()
_tracker_instance = None


def get_request_tracker():
    """Return the live request tracker instance if middleware is active."""
    with _tracker_instance_lock:
        return _tracker_instance


class RequestTrackingMiddleware(BaseHTTPMiddleware):
    """Middleware to track request metrics"""

    def __init__(self, app):
        super().__init__(app)
        self.request_count = 0
        self.error_count = 0
        self.total_latency = 0.0
        self._lock = threading.Lock()

        global _tracker_instance
        with _tracker_instance_lock:
            _tracker_instance = self

        if hasattr(app, "state"):
            app.state.request_tracker = self

    async def dispatch(self, request: Request, call_next: Callable):
        start_time = time.time()

        with self._lock:
            self.request_count += 1
            request_id = self.request_count

        response = None
        status_code = None
        error_happened = False
        latency = 0.0

        try:
            response = await call_next(request)
            status_code = response.status_code

        try:
            response.headers["X-Request-ID"] = str(request_id)
            response.headers["X-Processing-Time-Ms"] = str(int(latency * 1000))
        except Exception as header_exc:
            logger.warning("Failed to set tracking headers: %s", header_exc)

            raise
        finally:
            latency = time.time() - start_time

            with self._lock:
                self.total_latency += latency
                if error_happened or (status_code is not None and status_code >= 400):
                    self.error_count += 1

            if metrics_collector:
                result_label = "error" if error_happened or (status_code is not None and status_code >= 400) else "success"
                metric_labels = {"method": request.method, "result": result_label}
                metrics_collector.inc_counter("brain_ai_requests_total", labels=metric_labels)
                metrics_collector.observe_histogram(
                    "brain_ai_request_latency_seconds",
                    latency,
                    labels=metric_labels,
                )

        if response is not None:
            response.headers["X-Request-ID"] = str(request_id)
            response.headers["X-Processing-Time-Ms"] = str(int(latency * 1000))

        return response

    def get_stats(self):
        with self._lock:
            total_requests = self.request_count
            total_errors = self.error_count
            avg_latency = self.total_latency / total_requests if total_requests > 0 else 0.0

        return {
            "total_requests": total_requests,
            "total_errors": total_errors,
            "error_rate": total_errors / total_requests if total_requests > 0 else 0.0,
            "avg_latency_ms": int(avg_latency * 1000),
        }

