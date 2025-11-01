"""Pybind-powered bridge with in-memory fallback for document indexing/search."""

from __future__ import annotations

import importlib
import json
import logging
import os
import threading
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

import numpy as np

from .config import settings

LOGGER = logging.getLogger(__name__)


class MemoryIndex:
    """Lightweight in-process vector index for SAFE_MODE fallbacks."""

    def __init__(self) -> None:
        self._docs: Dict[str, Dict[str, object]] = {}
        self._lock = threading.Lock()

    def index_document(self, doc_id: str, embedding: Iterable[float], text: str) -> None:
        vector = np.asarray(list(embedding), dtype=np.float32)
        norm = float(np.linalg.norm(vector)) or 1.0
        vector = vector / norm

        with self._lock:
            self._docs[doc_id] = {
                "embedding": vector,
                "text": text,
            }

    def search(self, query_embedding: Iterable[float], top_k: int) -> List[Tuple[str, float]]:
        query = np.asarray(list(query_embedding), dtype=np.float32)
        norm = float(np.linalg.norm(query)) or 1.0
        query = query / norm

        with self._lock:
            items = list(self._docs.items())

        scores: List[Tuple[str, float]] = []
        for doc_id, payload in items:
            vec = payload["embedding"]
            score = float(np.dot(query, vec))
            scores.append((doc_id, score))

        scores.sort(key=lambda item: item[1], reverse=True)
        return scores[: max(1, top_k)]

    def get_text(self, doc_id: str) -> str | None:
        with self._lock:
            payload = self._docs.get(doc_id)
        if not payload:
            return None
        return payload.get("text")  # type: ignore[return-value]

    def size(self) -> int:
        with self._lock:
            return len(self._docs)

    def save(self, path: os.PathLike[str] | str) -> None:
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with self._lock:
            serialisable = {
                doc_id: {
                    "embedding": payload["embedding"].tolist(),
                    "text": payload["text"],
                }
                for doc_id, payload in self._docs.items()
            }
        with path.open("w", encoding="utf-8") as handle:
            json.dump(serialisable, handle)

    def load(self, path: os.PathLike[str] | str) -> None:
        path = Path(path)
        if not path.exists():
            return
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
        with self._lock:
            self._docs = {
                doc_id: {
                    "embedding": np.asarray(item["embedding"], dtype=np.float32),
                    "text": item["text"],
                }
                for doc_id, item in data.items()
            }


class CoreBridge:
    """Abstraction over the optional pybind11 module with SAFE_MODE fallback."""

    def __init__(self) -> None:
        self._memory = MemoryIndex()
        try:
            self._module = importlib.import_module("brain_ai_core")
            LOGGER.info("brain_ai_core module loaded")
        except ImportError:
            self._module = None
            LOGGER.warning("brain_ai_core module not available; using memory fallback")

        # Attempt to load snapshot on startup
        self.load_index(settings.index_snapshot_path)

    @property
    def available(self) -> bool:
        return self._module is not None

    def index_document(
        self, doc_id: str, text: str, embedding: Iterable[float]
    ) -> None:
        payload = list(embedding)
        if self._module:
            try:
                self._module.index_document(doc_id, text, payload)
            except TypeError:
                self._module.index_document(doc_id, text)
        self._memory.index_document(doc_id, payload, text)

    def search(
        self, query: str, top_k: int, embedding: Iterable[float]
    ) -> List[Tuple[str, float]]:
        payload = list(embedding)
        if self._module:
            try:
                results = self._module.search(query, top_k, payload)
            except TypeError:
                results = self._module.search(query, top_k)
            if results:
                return [(doc_id, float(score)) for doc_id, score in results]

        return self._memory.search(payload, top_k)

    def save_index(self, path: os.PathLike[str] | str) -> None:
        if self._module:
            try:
                self._module.save_index(str(path))
            except Exception as exc:  # pragma: no cover - defensive
                LOGGER.warning("Failed to save pybind index: %s", exc)
        self._memory.save(path)

    def load_index(self, path: os.PathLike[str] | str) -> None:
        if self._module:
            try:
                self._module.load_index(str(path))
            except Exception as exc:  # pragma: no cover - defensive
                LOGGER.warning("Failed to load pybind index: %s", exc)
        self._memory.load(path)

    def document_text(self, doc_id: str) -> str | None:
        return self._memory.get_text(doc_id)

    def size(self) -> int:
        return self._memory.size()


bridge = CoreBridge()


__all__ = ["CoreBridge", "bridge"]
