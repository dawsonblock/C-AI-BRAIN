"""Runtime configuration for the Brain-AI REST service."""

from __future__ import annotations

from dataclasses import dataclass
import os
from typing import Optional


def _to_bool(value: Optional[str], default: bool) -> bool:
    if value is None:
        return default
    value = value.strip().lower()
    if value in {"1", "true", "t", "yes", "on", "y"}:
        return True
    if value in {"0", "false", "f", "no", "off", "n"}:
        return False
    return default


def _to_int(value: Optional[str], default: int) -> int:
    if value is None:
        return default
    try:
        return int(value.strip())
    except (ValueError, TypeError):
        return default


@dataclass(frozen=True)
class Settings:
    safe_mode: bool
    llm_stub: bool
    embeddings_backend: str
    embedding_model_name: str
    embedding_seed: int
    max_doc_bytes: int
    max_query_tokens: int
    rate_limit_rpm: int
    require_api_key_for_writes: bool
    api_key: Optional[str]
    kill_switch_path: str
    index_snapshot_path: str
    metrics_enabled: bool
    structured_logging: bool
    http_client_timeout: int
    ocr_timeout: int
    llm_timeout: int
    request_timeout_seconds: int
    log_level: str


def _build_settings() -> Settings:
    safe_mode = _to_bool(os.getenv("SAFE_MODE"), True)
    llm_stub_env = _to_bool(os.getenv("LLM_STUB"), True)
    llm_api_key_present = bool(os.getenv("DEEPSEEK_API_KEY"))
    llm_stub = llm_stub_env or (safe_mode and not llm_api_key_present)

    embeddings_backend = os.getenv("EMBEDDINGS_BACKEND", "cpu").strip().lower()
    embedding_model_name = os.getenv(
        "EMBEDDING_MODEL_NAME",
        "sentence-transformers/all-MiniLM-L6-v2",
    )
    embedding_seed = _to_int(os.getenv("EMBEDDING_SEED"), 1337)

    max_doc_bytes = _to_int(os.getenv("MAX_DOC_BYTES"), 200_000)
    max_query_tokens = _to_int(os.getenv("MAX_QUERY_TOKENS"), 256)
    rate_limit_rpm = max(1, _to_int(os.getenv("RATE_LIMIT_RPM"), 120))

    require_api_key_for_writes = _to_bool(
        os.getenv("REQUIRE_API_KEY_FOR_WRITES"), True
    )
    api_key = os.getenv("API_KEY")

    kill_switch_path = os.getenv("KILL_PATH", "/tmp/brain.KILL")
    index_snapshot_path = os.getenv("INDEX_SNAPSHOT", "./data/index.json")

    metrics_enabled = _to_bool(os.getenv("METRICS_ENABLED"), True)
    structured_logging = _to_bool(os.getenv("STRUCTURED_LOGGING"), True)

    http_client_timeout = max(1, _to_int(os.getenv("HTTP_CLIENT_TIMEOUT"), 10))
    ocr_timeout = max(1, _to_int(os.getenv("OCR_TIMEOUT"), 5))
    llm_timeout = max(1, _to_int(os.getenv("LLM_TIMEOUT"), 15))
    request_timeout_seconds = max(1, _to_int(os.getenv("REQUEST_TIMEOUT_SECONDS"), 20))

    log_level = os.getenv("LOG_LEVEL", "INFO").strip().upper() or "INFO"

    return Settings(
        safe_mode=safe_mode,
        llm_stub=llm_stub,
        embeddings_backend=embeddings_backend,
        embedding_model_name=embedding_model_name,
        embedding_seed=embedding_seed,
        max_doc_bytes=max_doc_bytes,
        max_query_tokens=max_query_tokens,
        rate_limit_rpm=rate_limit_rpm,
        require_api_key_for_writes=require_api_key_for_writes,
        api_key=api_key,
        kill_switch_path=kill_switch_path,
        index_snapshot_path=index_snapshot_path,
        metrics_enabled=metrics_enabled,
        structured_logging=structured_logging,
        http_client_timeout=http_client_timeout,
        ocr_timeout=ocr_timeout,
        llm_timeout=llm_timeout,
        request_timeout_seconds=request_timeout_seconds,
        log_level=log_level,
    )


settings = _build_settings()


def reload_settings() -> Settings:
    global settings  # type: ignore[global-variable-not-assigned]
    settings = _build_settings()
    return settings


__all__ = ["Settings", "settings", "reload_settings"]
