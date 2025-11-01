"""DeepSeek API client with deterministic stub fallback."""

from __future__ import annotations

import json
import logging
import os
import time
from typing import Dict

import requests

from .config import settings

LOGGER = logging.getLogger(__name__)

DEFAULT_SYSTEM_PROMPT = "You are a grounded assistant. Answer only from provided context."


def _stub_response(prompt: str) -> Dict[str, object]:
    preview = " ".join(prompt.strip().split())[:120]
    heuristic = preview if preview else "No context"
    return {
        "answer": f"Answer: {heuristic} (stubbed)",
        "model": "stub",
        "latency_ms": 1,
    }


def ask_llm(prompt: str, system: str = DEFAULT_SYSTEM_PROMPT) -> Dict[str, object]:
    if settings.llm_stub or settings.safe_mode:
        return _stub_response(prompt)

    api_key = os.getenv("DEEPSEEK_API_KEY")
    if not api_key:
        LOGGER.warning("DEEPSEEK_API_KEY missing; using stub response")
        return _stub_response(prompt)

    url = os.getenv(
        "DEEPSEEK_URL",
        "https://api.deepseek.com/v1/chat/completions",
    )
    model_name = os.getenv("DEEPSEEK_MODEL", "deepseek-chat")
    try:
        temperature = float(os.getenv("LLM_TEMP", "0.2"))
    except ValueError:
        temperature = 0.2

    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json",
    }
    payload = {
        "model": model_name,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": prompt},
        ],
        "temperature": temperature,
    }

    started = time.time()
    try:
        response = requests.post(
            url,
            data=json.dumps(payload),
            headers=headers,
            timeout=settings.llm_timeout,
        )
        response.raise_for_status()
        data = response.json()
        content = data["choices"][0]["message"]["content"]
        latency = int((time.time() - started) * 1000)
        return {"answer": content, "model": model_name, "latency_ms": latency}
    except requests.Timeout:
        LOGGER.error("LLM timeout after %ss", settings.llm_timeout)
        return {
            "answer": "LLM timeout",
            "model": model_name,
            "latency_ms": int((time.time() - started) * 1000),
            "error": "timeout",
        }
    except requests.HTTPError as exc:
        LOGGER.error("LLM HTTP error: %s", exc)
        return {
            "answer": f"LLM HTTP error: {exc}",
            "model": model_name,
            "latency_ms": int((time.time() - started) * 1000),
            "error": "http_error",
        }
    except Exception as exc:  # pragma: no cover - defensive
        LOGGER.exception("LLM request failed")
        return {
            "answer": f"LLM failure: {exc}",
            "model": model_name,
            "latency_ms": int((time.time() - started) * 1000),
            "error": "unexpected",
        }


__all__ = ["ask_llm"]
