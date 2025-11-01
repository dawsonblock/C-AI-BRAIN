"""
Rate limiting middleware for API endpoints
"""

import time
import logging
from collections import defaultdict
from typing import Dict, Tuple, AsyncGenerator

from fastapi import Request, HTTPException, status

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
    
    def check_rate_limit(self, client_ip: str) -> Tuple[bool, Dict[str, int]]:
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


async def enforce_rate_limits(request: Request) -> None:
    """FastAPI dependency to enforce per-IP rate limits."""
    if rate_limiter is None:
        return

    client_ip = request.client.host if request.client else "anonymous"

    # Skip health/status endpoints
    if request.url.path.endswith("/health") or request.url.path.endswith("/status"):
        return

    allowed, info = rate_limiter.check_rate_limit(client_ip)

    if not allowed:
        logger.warning(
            "Rate limit exceeded for %s, retry_after=%ss",
            client_ip,
            info["retry_after"],
        )
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail={
                "error": "Rate limit exceeded",
                "retry_after_seconds": info["retry_after"],
            },
            headers={
                "Retry-After": str(info["retry_after"]),
                "X-RateLimit-Remaining": "0",
                "X-RateLimit-Reset": str(info["reset_seconds"]),
            },
        )

    request.state.rate_limit_info = info


async def enforce_concurrency(request: Request) -> AsyncGenerator[None, None]:
    """FastAPI dependency to cap concurrent in-flight requests per client."""
    if concurrency_limiter is None:
        yield
        return

    client_ip = request.client.host if request.client else "anonymous"

    allowed = await concurrency_limiter.acquire(client_ip)
    if not allowed:
        logger.warning("Concurrency limit exceeded for %s", client_ip)
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail={"error": "Too many concurrent requests", "client": client_ip},
        )

    try:
        yield
    finally:
        await concurrency_limiter.release(client_ip)

