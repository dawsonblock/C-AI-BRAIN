"""Embedding backend selector."""

from __future__ import annotations

import logging
from typing import List

from .config import settings

LOGGER = logging.getLogger(__name__)


def _cpu_embed(text: str) -> List[float]:
    from .embeddings_local import embed_text as local_embed

    return local_embed(text)


def _external_embed(text: str) -> List[float]:  # pragma: no cover - optional backend
    if settings.safe_mode:
        raise RuntimeError("External embeddings disabled in SAFE_MODE")
    raise NotImplementedError(
        "External embedding backend not configured; set EMBEDDINGS_BACKEND=cpu"
    )


def embed_text(text: str) -> List[float]:
    backend = settings.embeddings_backend
    if backend == "cpu":
        return _cpu_embed(text)

    if backend == "external":
        return _external_embed(text)

    LOGGER.warning("Unknown EMBEDDINGS_BACKEND=%s; falling back to CPU", backend)
    return _cpu_embed(text)


__all__ = ["embed_text"]
