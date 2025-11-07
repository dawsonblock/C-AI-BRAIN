"""Middleware for request processing, rate limiting, and security."""

import time
import uuid
from typing import Callable
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response, JSONResponse
from slowapi import Limiter
from slowapi.util import get_remote_address
from slowapi.errors import RateLimitExceeded
import logging

from .config import settings
from .logging_setup import request_id_context

logger = logging.getLogger(__name__)


class RequestIdMiddleware(BaseHTTPMiddleware):
    """Add unique request ID to each request."""
    
    async def dispatch(
        self, 
        request: Request, 
        call_next: Callable
    ) -> Response:
        """Process request with request ID."""
        request_id = request.headers.get("X-Request-ID", str(uuid.uuid4()))
        request.state.request_id = request_id
        
        # Set in context for logging
        request_id_context.set(request_id)
        
        response = await call_next(request)
        response.headers["X-Request-ID"] = request_id
        
        return response


class RequestLoggingMiddleware(BaseHTTPMiddleware):
    """Log all requests with timing information."""
    
    async def dispatch(
        self,
        request: Request,
        call_next: Callable
    ) -> Response:
        """Process and log request."""
        start_time = time.time()
        
        # Log request
        logger.info(
            "Request started",
            extra={
                "method": request.method,
                "path": request.url.path,
                "query": str(request.url.query),
                "client": request.client.host if request.client else "unknown",
                "user_agent": request.headers.get("user-agent", ""),
            }
        )
        
        # Process request
        try:
            response = await call_next(request)
        except Exception as e:
            duration_ms = (time.time() - start_time) * 1000
            logger.error(
                f"Request failed: {type(e).__name__}",
                extra={
                    "method": request.method,
                    "path": request.url.path,
                    "duration_ms": duration_ms,
                    "error": str(e),
                },
                exc_info=True
            )
            raise
        
        # Log response
        duration_ms = (time.time() - start_time) * 1000
        logger.info(
            "Request completed",
            extra={
                "method": request.method,
                "path": request.url.path,
                "status_code": response.status_code,
                "duration_ms": duration_ms,
            }
        )
        
        # Add timing header
        response.headers["X-Response-Time"] = f"{duration_ms:.2f}ms"
        
        return response


class BodySizeLimitMiddleware(BaseHTTPMiddleware):
    """Enforce request body size limits."""
    
    def __init__(self, app, max_size: int = 1_000_000):
        """
        Initialize middleware.
        
        Args:
            app: ASGI application
            max_size: Maximum body size in bytes
        """
        super().__init__(app)
        self.max_size = max_size
    
    async def dispatch(
        self,
        request: Request,
        call_next: Callable
    ) -> Response:
        """Check request size and process."""
        content_length = request.headers.get("content-length")
        
        if content_length and int(content_length) > self.max_size:
            logger.warning(
                "Request body too large",
                extra={
                    "content_length": content_length,
                    "max_size": self.max_size,
                    "path": request.url.path,
                    "client": request.client.host if request.client else "unknown",
                }
            )
            return JSONResponse(
                status_code=413,
                content={
                    "error": "Request body too large",
                    "detail": f"Maximum size: {self.max_size} bytes",
                    "request_id": getattr(request.state, "request_id", "unknown")
                }
            )
        
        return await call_next(request)


class SecurityHeadersMiddleware(BaseHTTPMiddleware):
    """Add security headers to responses."""
    
    async def dispatch(
        self,
        request: Request,
        call_next: Callable
    ) -> Response:
        """Add security headers."""
        response = await call_next(request)
        
        # Security headers
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["X-XSS-Protection"] = "1; mode=block"
        response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin"
        
        # HSTS if enabled
        if settings.hsts_enabled:
            response.headers["Strict-Transport-Security"] = (
                "max-age=31536000; includeSubDomains"
            )
        
        # Content Security Policy for API
        response.headers["Content-Security-Policy"] = (
            "default-src 'none'; frame-ancestors 'none'"
        )
        
        return response


# Rate limiter setup
limiter = Limiter(
    key_func=get_remote_address,
    default_limits=[settings.rate_limit_default] if settings.rate_limit_enabled else [],
    enabled=settings.rate_limit_enabled,
    headers_enabled=True,
)


def rate_limit_exceeded_handler(request: Request, exc: RateLimitExceeded) -> JSONResponse:
    """Handle rate limit exceeded errors."""
    logger.warning(
        "Rate limit exceeded",
        extra={
            "client": request.client.host if request.client else "unknown",
            "path": request.url.path,
            "limit": exc.detail,
        }
    )
    return JSONResponse(
        status_code=429,
        content={
            "error": "Rate limit exceeded",
            "detail": exc.detail,
            "request_id": getattr(request.state, "request_id", "unknown")
        },
        headers={"Retry-After": "60"}
    )
