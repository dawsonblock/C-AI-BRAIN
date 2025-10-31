"""
Rate limiting middleware for API endpoints
"""

import time
import logging
from collections import defaultdict
from typing import Dict, Tuple
from fastapi import Request, HTTPException
from fastapi.responses import JSONResponse

logger = logging.getLogger(__name__)


class RateLimiter:
    """Token bucket rate limiter"""
    
    def __init__(
        self,
        requests_per_minute: int = 60,
        burst_size: int = 10
    ):
        """
        Initialize rate limiter
        
        Args:
            requests_per_minute: Sustained rate limit
            burst_size: Maximum burst size
        """
        self.requests_per_minute = requests_per_minute
        self.burst_size = burst_size
        self.refill_rate = requests_per_minute / 60.0  # tokens per second
        
        # Token buckets per client IP
        self.buckets: Dict[str, Tuple[float, float]] = defaultdict(
            lambda: (burst_size, time.time())
        )
        
        logger.info(
            f"✅ Rate limiter initialized: "
            f"{requests_per_minute}/min, burst={burst_size}"
        )
    
    def check_rate_limit(self, client_ip: str) -> Tuple[bool, Dict]:
        """
        Check if request is within rate limit
        
        Returns:
            (allowed, info_dict)
        """
        now = time.time()
        tokens, last_update = self.buckets[client_ip]
        
        # Refill tokens based on time elapsed
        elapsed = now - last_update
        tokens = min(
            self.burst_size,
            tokens + elapsed * self.refill_rate
        )
        
        # Check if we have tokens available
        if tokens >= 1.0:
            # Consume 1 token
            tokens -= 1.0
            self.buckets[client_ip] = (tokens, now)
            
            return True, {
                "allowed": True,
                "remaining": int(tokens),
                "reset_seconds": int((self.burst_size - tokens) / self.refill_rate)
            }
        else:
            # Rate limit exceeded
            self.buckets[client_ip] = (tokens, now)
            
            return False, {
                "allowed": False,
                "remaining": 0,
                "reset_seconds": int((1.0 - tokens) / self.refill_rate),
                "retry_after": int((1.0 - tokens) / self.refill_rate)
            }
    
    async def __call__(self, request: Request):
        """Middleware callable"""
        # Get client IP
        client_ip = request.client.host
        
        # Skip rate limiting for health checks
        if request.url.path.endswith("/health") or request.url.path.endswith("/status"):
            return None
        
        # Check rate limit
        allowed, info = self.check_rate_limit(client_ip)
        
        if not allowed:
            logger.warning(
                f"Rate limit exceeded for {client_ip}, "
                f"retry_after={info['retry_after']}s"
            )
            
            return JSONResponse(
                status_code=429,
                content={
                    "error": "Rate limit exceeded",
                    "retry_after_seconds": info["retry_after"]
                },
                headers={
                    "X-RateLimit-Remaining": "0",
                    "X-RateLimit-Reset": str(info["reset_seconds"]),
                    "Retry-After": str(info["retry_after"])
                }
            )
        
        # Add rate limit headers to response
        request.state.rate_limit_info = info
        return None


class ConcurrencyLimiter:
    """Limit concurrent requests per client"""
    
    def __init__(self, max_concurrent: int = 5):
        self.max_concurrent = max_concurrent
        self.active_requests: Dict[str, int] = defaultdict(int)
        
        logger.info(f"✅ Concurrency limiter initialized: max={max_concurrent}")
    
    async def acquire(self, client_ip: str) -> bool:
        """Acquire slot for request"""
        if self.active_requests[client_ip] >= self.max_concurrent:
            return False
        
        self.active_requests[client_ip] += 1
        return True
    
    async def release(self, client_ip: str):
        """Release slot after request"""
        self.active_requests[client_ip] = max(
            0, self.active_requests[client_ip] - 1
        )


# Global rate limiter instances
rate_limiter = None
concurrency_limiter = None


def init_rate_limiters(
    requests_per_minute: int = 60,
    burst_size: int = 10,
    max_concurrent: int = 5
):
    """Initialize rate limiters"""
    global rate_limiter, concurrency_limiter
    
    rate_limiter = RateLimiter(requests_per_minute, burst_size)
    concurrency_limiter = ConcurrencyLimiter(max_concurrent)
    
    logger.info("✅ Rate limiters initialized")

