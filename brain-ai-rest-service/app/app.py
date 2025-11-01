"""Hardened FastAPI application for Brain-AI."""

from __future__ import annotations

import logging
import os
import time
from pathlib import Path
from typing import Dict, List

from fastapi import FastAPI, HTTPException, Request, Response, status
from fastapi.middleware.cors import CORSMiddleware

from .config import settings
from .core_bridge import bridge
from .embeddings import embed_text
from .logging_utils import configure_logging
from .metrics import (
    ERROR_COUNT,
    HTTP_REQUESTS,
    INDEXED_DOCUMENTS,
    INDEX_SIZE,
    QUERY_LATENCY,
    REQUEST_LATENCY,
    collect_metrics,
)
from .rate_limiter import RateLimiter
from .schemas import FactPayload, IndexPayload, QueryPayload, QueryResponse
from .llm_deepseek import ask_llm

LOGGER = logging.getLogger(__name__)

app = FastAPI(title="Brain-AI REST", version="2.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_methods=["*"],
    allow_headers=["*"],
)

rate_limiter = RateLimiter(settings.rate_limit_rpm)
FACT_STORE: Dict[str, Dict[str, object]] = {}


def _client_ip(request: Request) -> str:
    if request.client and request.client.host:
        return request.client.host
    return "unknown"


def _require_api_key(request: Request) -> None:
    if not settings.require_api_key_for_writes:
        return
    expected = settings.api_key
    if not expected:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="API key not configured",
        )

    candidate = request.headers.get("X-API-Key")
    if not candidate:
        auth = request.headers.get("Authorization", "")
        if auth.startswith("Bearer "):
            candidate = auth[7:]

    if not candidate:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Missing API key")

    if candidate != expected:
        raise HTTPException(status.HTTP_403_FORBIDDEN, "Invalid API key")


def _check_kill_switch() -> None:
    if Path(settings.kill_switch_path).exists():
        raise HTTPException(status.HTTP_503_SERVICE_UNAVAILABLE, "Kill switch engaged")


@app.on_event("startup")
async def startup_event() -> None:
    configure_logging()
    LOGGER.info(
        "Brain-AI REST starting (safe_mode=%s, llm_stub=%s, pybind=%s)",
        settings.safe_mode,
        settings.llm_stub,
        bridge.available,
    )
    Path(settings.kill_switch_path).parent.mkdir(parents=True, exist_ok=True)


@app.middleware("http")
async def observability_middleware(request: Request, call_next):
    route = request.url.path
    start = time.perf_counter()
    client = _client_ip(request)

    status_code = status.HTTP_200_OK
    try:
        rate_limiter.check(client)
        _check_kill_switch()

        if request.method in {"POST", "PUT", "PATCH"}:
            content_length = request.headers.get("content-length")
            if content_length and int(content_length) > settings.max_doc_bytes:
                raise HTTPException(
                    status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
                    "Payload too large",
                )

        response = await call_next(request)
        status_code = response.status_code
        return response
    except HTTPException as exc:
        status_code = exc.status_code
        raise
    except Exception as exc:  # pragma: no cover - defensive
        status_code = status.HTTP_500_INTERNAL_SERVER_ERROR
        ERROR_COUNT.labels(component="server").inc()
        LOGGER.exception("Unhandled error")
        raise
    finally:
        duration = time.perf_counter() - start
        HTTP_REQUESTS.labels(route=route, status=str(status_code)).inc()
        REQUEST_LATENCY.labels(route=route).observe(duration)
        LOGGER.info(
            "request complete",
            extra={"route": route, "lat_ms": int(duration * 1000), "status_code": status_code, "client_ip": client},
        )


@app.get("/healthz")
async def healthz() -> Dict[str, object]:
    return {
        "ok": True,
        "safe_mode": settings.safe_mode,
        "llm_stub": settings.llm_stub,
        "pybind_available": bridge.available,
        "documents": bridge.size(),
    }


@app.get("/readyz")
async def readyz() -> Dict[str, object]:
    _check_kill_switch()
    return {
        "ready": True,
        "requires_api_key": settings.require_api_key_for_writes,
    }


@app.get("/metrics")
async def metrics() -> Response:
    if not settings.metrics_enabled:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Metrics disabled")
    payload = collect_metrics()
    return Response(payload, media_type="text/plain; version=0.0.4")


@app.post("/index", response_model=Dict[str, bool])
async def index_document(payload: IndexPayload, request: Request) -> Dict[str, bool]:
    _require_api_key(request)
    start = time.perf_counter()

    embedding = embed_text(payload.text)
    bridge.index_document(payload.doc_id, payload.text, embedding)
    INDEXED_DOCUMENTS.inc()
    INDEX_SIZE.set(bridge.size())
    bridge.save_index(settings.index_snapshot_path)

    duration_ms = int((time.perf_counter() - start) * 1000)
    LOGGER.info(
        "indexed document",
        extra={"route": "/index", "lat_ms": duration_ms, "doc_id": payload.doc_id},
    )
    return {"ok": True}


def _build_context(hits: List[Dict[str, object]]) -> str:
    lines = []
    for hit in hits:
        lines.append(f"[{hit['doc_id']}] score={hit['score']:.3f}\n{hit['text']}")
    return "\n\n".join(lines) if lines else "No matching context."


@app.post("/query", response_model=QueryResponse)
async def query(payload: QueryPayload, request: Request) -> QueryResponse:
    start = time.perf_counter()

    embedding = embed_text(payload.query)
    raw_hits = bridge.search(payload.query, payload.top_k, embedding)

    enriched_hits: List[Dict[str, object]] = []
    for doc_id, score in raw_hits:
        text = bridge.document_text(doc_id) or "(text unavailable)"
        enriched_hits.append({"doc_id": doc_id, "score": score, "text": text})

    answer = ask_llm(
        prompt=f"Question: {payload.query}\n\nContext:\n{_build_context(enriched_hits)}\nAnswer:"
    )

    duration = time.perf_counter() - start
    QUERY_LATENCY.observe(duration)

    if "error" in answer:
        ERROR_COUNT.labels(component="llm").inc()

    return QueryResponse(
        answer=answer.get("answer", ""),
        hits=enriched_hits,
        model=str(answer.get("model", "stub")),
        latency_ms=int(answer.get("latency_ms", duration * 1000)),
    )


@app.post("/facts", response_model=Dict[str, object])
async def upsert_fact(payload: FactPayload, request: Request) -> Dict[str, object]:
    _require_api_key(request)
    FACT_STORE[payload.question] = payload.model_dump()
    return {"ok": True, "stored": len(FACT_STORE)}


@app.get("/facts", response_model=Dict[str, object])
async def list_facts(request: Request) -> Dict[str, object]:
    _require_api_key(request)
    return {"facts": FACT_STORE, "count": len(FACT_STORE)}


@app.post("/admin/kill")
async def trigger_kill_switch(request: Request) -> Dict[str, object]:
    _require_api_key(request)
    Path(settings.kill_switch_path).touch()
    return {"ok": True, "path": settings.kill_switch_path}


@app.delete("/admin/kill")
async def clear_kill_switch(request: Request) -> Dict[str, object]:
    _require_api_key(request)
    try:
        os.remove(settings.kill_switch_path)
    except FileNotFoundError:
        pass
    return {"ok": True}


def create_app() -> FastAPI:
    return app


__all__ = ["app", "create_app"]
