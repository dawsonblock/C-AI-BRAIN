"""Structured logging utilities for the Brain-AI REST service."""

from __future__ import annotations

import json
import logging
import socket
import sys
import time
from typing import Any, Dict

from .config import settings


class JsonFormatter(logging.Formatter):
    """Format log records as single-line JSON objects."""

    def format(self, record: logging.LogRecord) -> str:  # noqa: D401
        payload: Dict[str, Any] = {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(record.created)),
            "lvl": record.levelname,
            "logger": record.name,
            "host": socket.gethostname(),
            "event": record.getMessage(),
        }

        if record.exc_info:
            payload["exc_info"] = self.formatException(record.exc_info)

        if hasattr(record, "route"):
            payload["route"] = getattr(record, "route")

        if hasattr(record, "lat_ms"):
            payload["lat_ms"] = getattr(record, "lat_ms")

        if hasattr(record, "client_ip"):
            payload["client_ip"] = getattr(record, "client_ip")

        if hasattr(record, "status_code"):
            payload["status_code"] = getattr(record, "status_code")

        return json.dumps(payload, ensure_ascii=False)


def configure_logging() -> None:
    """Configure root logging for the service."""

    level_name = settings.log_level.upper()
    level = getattr(logging, level_name, logging.INFO)

    root = logging.getLogger()
    root.setLevel(level)

    if settings.structured_logging:
        formatter: logging.Formatter = JsonFormatter()
    else:
        formatter = logging.Formatter(
            "%(asctime)s | %(levelname)s | %(name)s | %(message)s"
        )

    handler = logging.StreamHandler(sys.stdout)
    handler.setLevel(level)
    handler.setFormatter(formatter)

    root.handlers = [handler]

    # Reduce overly chatty loggers
    for noisy in ("uvicorn", "uvicorn.access", "asyncio", "httpx"):
        logging.getLogger(noisy).setLevel(max(level, logging.WARNING))


__all__ = ["configure_logging"]
