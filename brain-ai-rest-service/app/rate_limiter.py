"""Simple in-process rate limiting utilities."""

from __future__ import annotations

import threading
import time
from collections import deque
from typing import Deque, Dict

from fastapi import HTTPException, status


class RateLimiter:
    """Token-bucket style limiter for per-IP enforcement."""

    def __init__(self, requests_per_minute: int) -> None:
        if requests_per_minute <= 0:
            raise ValueError("requests_per_minute must be > 0")
        self._limit = requests_per_minute
        self._window_seconds = 60.0
        self._lock = threading.Lock()
        self._events: Dict[str, Deque[float]] = {}

    def check(self, client_id: str) -> None:
        now = time.time()
        with self._lock:
            bucket = self._events.setdefault(client_id, deque())
            # Trim window
            while bucket and now - bucket[0] > self._window_seconds:
                bucket.popleft()

            if len(bucket) >= self._limit:
                retry_after = max(0.0, self._window_seconds - (now - bucket[0]))
                headers = {"Retry-After": str(int(retry_after))}
                raise HTTPException(
                    status_code=status.HTTP_429_TOO_MANY_REQUESTS,
                    detail="Too many requests",
                    headers=headers,
                )

            bucket.append(now)


__all__ = ["RateLimiter"]
