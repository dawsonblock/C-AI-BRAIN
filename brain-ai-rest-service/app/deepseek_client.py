import logging
from typing import Any, Dict, List

import httpx

from .config import settings

logger = logging.getLogger(__name__)


class DeepSeekClientError(Exception):
    pass


async def deepseek_chat_raw(messages: List[Dict[str, str]], **extra: Any) -> Dict[str, Any]:
    if not settings.deepseek_api_base or not settings.deepseek_api_key:
        raise DeepSeekClientError("DeepSeek API not configured")
    url = settings.deepseek_api_base.rstrip("/") + settings.deepseek_chat_path
    payload: Dict[str, Any] = {"model": settings.deepseek_model, "messages": messages}
    if extra:
        payload.update(extra)
    timeout = settings.deepseek_timeout_seconds
    max_retries = settings.deepseek_max_retries
    last_error: Exception | None = None
    for _ in range(max_retries):
        try:
            async with httpx.AsyncClient(timeout=timeout) as client:
                response = await client.post(
                    url,
                    headers={
                        "Authorization": f"Bearer {settings.deepseek_api_key}",
                        "Content-Type": "application/json",
                    },
                    json=payload,
                )
            if response.status_code >= 500:
                last_error = DeepSeekClientError(
                    f"DeepSeek API server error: {response.status_code}"
                )
                continue
            if response.status_code != 200:
                raise DeepSeekClientError(
                    f"DeepSeek API error: {response.status_code} {response.text}"
                )
            data = response.json()
            return data
        except (httpx.RequestError, httpx.TimeoutException) as exc:
            last_error = exc
            logger.warning("DeepSeek request failed, retrying", extra={"error": str(exc)})
    if last_error is None:
        raise DeepSeekClientError("DeepSeek API request failed")
    raise DeepSeekClientError(f"DeepSeek API request failed after retries: {last_error}")
