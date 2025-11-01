"""Python convenience layer for the Brain-AI pybind11 bindings."""

from __future__ import annotations

import importlib
from typing import Any


def _load_extension() -> Any:
    try:
        return importlib.import_module("brain_ai_py")
    except ImportError as exc:  # pragma: no cover - surfaced during packaging issues
        raise ImportError(
            "brain_ai_py extension module is not available."
            " Ensure the wheel was built with BUILD_PYTHON_BINDINGS=ON."
        ) from exc


_ext = _load_extension()

FusionWeights = _ext.FusionWeights
QueryConfig = _ext.QueryConfig
ScoredResult = _ext.ScoredResult
QueryResponse = _ext.QueryResponse
CognitiveHandler = _ext.CognitiveHandler

__all__ = [
    "CognitiveHandler",
    "FusionWeights",
    "QueryConfig",
    "QueryResponse",
    "ScoredResult",
]

__version__ = getattr(_ext, "__version__", "0.0.0")
