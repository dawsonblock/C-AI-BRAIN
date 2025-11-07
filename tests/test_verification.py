"""Tests for verification utilities."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import Mock, patch

import pytest

import sys

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SERVICE_ROOT = PROJECT_ROOT / "brain-ai-rest-service"
sys.path.insert(0, str(SERVICE_ROOT))

from app import verification


def test_safe_code_sandbox_rejects_without_hardened_executor(monkeypatch: pytest.MonkeyPatch) -> None:
    """Sandbox should refuse to run when no hardened executor is configured."""

    monkeypatch.delenv("ENABLE_CODE_SANDBOX", raising=False)
    monkeypatch.delenv("HARDENED_SANDBOX_URL", raising=False)

    result = verification.safe_code_sandbox("print('hello')")

    assert "error" in result
    assert "disabled" in result["error"].lower()


def test_safe_code_sandbox_requires_executor_url(monkeypatch: pytest.MonkeyPatch) -> None:
    """Sandbox should instruct callers to configure the hardened executor endpoint."""

    monkeypatch.setenv("ENABLE_CODE_SANDBOX", "true")
    monkeypatch.delenv("HARDENED_SANDBOX_URL", raising=False)

    result = verification.safe_code_sandbox("print('hello')")

    assert "error" in result
    assert "hardened sandbox executor" in result["error"].lower()


def test_safe_code_sandbox_proxies_to_remote_executor(monkeypatch: pytest.MonkeyPatch) -> None:
    """Sandbox should call the hardened executor when configured."""

    monkeypatch.setenv("ENABLE_CODE_SANDBOX", "true")
    monkeypatch.setenv("HARDENED_SANDBOX_URL", "https://sandbox.example/api/run")

    mock_response = Mock()
    mock_response.raise_for_status.return_value = None
    mock_response.json.return_value = {
        "stdout": "done",
        "stderr": "",
        "returncode": 0,
        "success": True,
    }

    with patch("app.verification.requests.post", return_value=mock_response) as mock_post:
        result = verification.safe_code_sandbox("print('hello')", timeout=3, max_output_bytes=5)

    mock_post.assert_called_once()
    called_args, called_kwargs = mock_post.call_args
    assert called_args[0] == "https://sandbox.example/api/run"
    assert called_kwargs["json"]["code"] == "print('hello')"
    assert called_kwargs["json"]["timeout"] == 3
    assert called_kwargs["timeout"] >= 5  # min enforced timeout buffer

    assert result == {
        "stdout": "done",
        "stderr": "",
        "returncode": 0,
        "success": True,
    }
