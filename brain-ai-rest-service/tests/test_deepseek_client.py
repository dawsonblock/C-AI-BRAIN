import httpx
import pytest

from app.config import settings
from app.deepseek_client import DeepSeekClientError, deepseek_chat_raw


@pytest.mark.asyncio
async def test_deepseek_not_configured_raises():
    original_base = settings.deepseek_api_base
    original_key = settings.deepseek_api_key
    settings.deepseek_api_base = ""
    settings.deepseek_api_key = ""
    try:
        with pytest.raises(DeepSeekClientError):
            await deepseek_chat_raw([{"role": "user", "content": "hi"}])
    finally:
        settings.deepseek_api_base = original_base
        settings.deepseek_api_key = original_key


class _DummyResponse:
    def __init__(self, status_code: int, data: dict):
        self.status_code = status_code
        self._data = data

    def json(self) -> dict:
        return self._data

    @property
    def text(self) -> str:
        return ""


@pytest.mark.asyncio
async def test_deepseek_chat_raw_success(monkeypatch):
    async def fake_post(self, url, headers=None, json=None):
        return _DummyResponse(200, {"ok": True, "payload": json})

    settings.deepseek_api_base = "https://example.com"
    settings.deepseek_api_key = "test-key"
    settings.deepseek_chat_path = "/chat"
    settings.deepseek_max_retries = 1

    monkeypatch.setattr(httpx.AsyncClient, "post", fake_post)

    result = await deepseek_chat_raw([{"role": "user", "content": "hi"}])
    assert result["ok"] is True
    assert result["payload"]["model"] == settings.deepseek_model
