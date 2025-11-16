"""LLM routing abstraction.

Currently supports DeepSeek as the primary provider, with a simple
interface that can be extended to additional backends.
"""

from typing import Any, Dict, List
import logging

from .config import settings
from .deepseek_client import deepseek_chat_raw, DeepSeekClientError

logger = logging.getLogger(__name__)


class LLMRouterError(Exception):
    """Raised when the LLM router cannot fulfill a request."""


async def llm_chat(
    messages: List[Dict[str, str]],
    provider: str | None = None,
    **extra: Any,
) -> Dict[str, Any]:
    """Route chat request to the configured LLM backend.

    Args:
        messages: List of chat messages (role/content pairs).
        provider: Optional override provider; if None uses settings.llm_provider.
        **extra: Extra parameters passed through to the backend.

    Returns:
        Raw response payload from the backend.

    Raises:
        LLMRouterError: If the provider is unsupported or underlying
            backend call fails.
    """

    backend = (provider or settings.llm_provider).lower()

    try:
        if backend == "deepseek":
            return await deepseek_chat_raw(messages, **extra)
        raise LLMRouterError(f"Unsupported LLM provider: {backend}")
    except DeepSeekClientError as exc:
        logger.error("DeepSeek backend failed in llm_router", extra={"error": str(exc)})
        raise LLMRouterError(str(exc)) from exc
