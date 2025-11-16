import pytest

from app.config import settings
from app.llm_router import llm_chat, LLMRouterError


@pytest.mark.asyncio
async def test_llm_chat_unsupported_provider():
    with pytest.raises(LLMRouterError):
        await llm_chat([{"role": "user", "content": "hi"}], provider="unknown")


@pytest.mark.asyncio
async def test_llm_chat_uses_default_provider(monkeypatch):
    async def fake_llm_chat(messages, provider=None, **extra):
        # This test will override settings.llm_provider to a dummy value
        # and ensure the router respects it; we simulate success.
        return {"ok": True, "provider": provider or settings.llm_provider}

    original_provider = settings.llm_provider
    settings.llm_provider = "deepseek"
    try:
        # Instead of patching deepseek_client directly, rely on the router
        # behavior: when provider is None, settings.llm_provider is used.
        # We call the public llm_chat and expect it to route accordingly.
        # Here we patch llm_chat recursively via monkeypatch to simulate
        # a backend implementation.
        monkeypatch.setattr("app.llm_router.deepseek_chat_raw", lambda *a, **k: {"ok": True})
        result = await llm_chat([{"role": "user", "content": "hi"}])
        assert result["ok"] is True
    finally:
        settings.llm_provider = original_provider


@pytest.mark.asyncio
async def test_llm_chat_deepseek_error(monkeypatch):
    from app.llm_router import DeepSeekClientError

    async def failing_backend(*args, **kwargs):  # type: ignore[override]
        raise DeepSeekClientError("backend failure")

    monkeypatch.setattr("app.llm_router.deepseek_chat_raw", failing_backend)

    with pytest.raises(LLMRouterError):
        await llm_chat([{"role": "user", "content": "hi"}], provider="deepseek")
