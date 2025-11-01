"""
Middleware for Brain-AI REST API
- API key authentication
- Request tracking
- Rate limiting integration
"""
import os
import time
import logging
from fastapi import Request, HTTPException, status
from fastapi.responses import JSONResponse
from starlette.middleware.base import BaseHTTPMiddleware
from typing import Callable

logger = logging.getLogger(__name__)


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


class RequestTrackingMiddleware(BaseHTTPMiddleware):
    """Middleware to track request metrics"""
    
    def __init__(self, app):
        super().__init__(app)
        self.request_count = 0
        self.error_count = 0
        self.total_latency = 0.0
    
    async def dispatch(self, request: Request, call_next: Callable):
        start_time = time.time()
        self.request_count += 1
        
        try:
            response = await call_next(request)
            
            # Track metrics
            latency = time.time() - start_time
            self.total_latency += latency
            
            if response.status_code >= 400:
                self.error_count += 1
            
            # Add custom headers
            response.headers["X-Request-ID"] = str(self.request_count)
            response.headers["X-Processing-Time-Ms"] = str(int(latency * 1000))
            
            return response
            
        except Exception as e:
            self.error_count += 1
            logger.error(f"Request failed: {e}")
            raise
    
    def get_stats(self):
        avg_latency = self.total_latency / self.request_count if self.request_count > 0 else 0
        return {
            "total_requests": self.request_count,
            "total_errors": self.error_count,
            "error_rate": self.error_count / self.request_count if self.request_count > 0 else 0,
            "avg_latency_ms": int(avg_latency * 1000)
        }

