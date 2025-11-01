"""Deterministic CPU embedding fallback using sentence-transformers when available."""

from __future__ import annotations

import hashlib
import logging
from functools import lru_cache
from typing import List

import numpy as np

from .config import settings

LOGGER = logging.getLogger(__name__)

EMBEDDING_DIM = 384


@lru_cache(maxsize=1)
def _load_model():
    from sentence_transformers import SentenceTransformer  # type: ignore

    LOGGER.info(
        "Loading sentence-transformers model %s for CPU embeddings",
        settings.embedding_model_name,
    )
    model = SentenceTransformer(settings.embedding_model_name, device="cpu")
    return model


def _fallback_embed(text: str) -> np.ndarray:
    digest = hashlib.sha256(text.encode("utf-8")).digest()
    seed = int.from_bytes(digest[:8], "big") ^ settings.embedding_seed
    rng = np.random.default_rng(seed)
    vector = rng.standard_normal(EMBEDDING_DIM).astype(np.float32)
    norm = float(np.linalg.norm(vector)) or 1.0
    return vector / norm


def embed_text(text: str) -> List[float]:
    """Return a deterministic embedding for ``text`` using CPU resources only."""

    try:
        model = _load_model()
        vector = model.encode(text, device="cpu", normalize_embeddings=True)
        array = np.asarray(vector, dtype=np.float32)
        if array.ndim == 0:
            array = array.reshape(1)
        if array.ndim > 1:
            array = array[0]
        return array.tolist()
    except Exception as exc:  # pragma: no cover - fallback path
        LOGGER.warning("Falling back to deterministic RNG embeddings: %s", exc)
        return _fallback_embed(text).tolist()


__all__ = ["embed_text", "EMBEDDING_DIM"]
