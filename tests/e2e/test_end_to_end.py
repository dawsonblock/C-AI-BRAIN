import os
import signal
import subprocess
import sys
import time
from pathlib import Path

import pytest
import requests


PORT = 5801


def wait_ready(port: int, timeout: float = 15.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            response = requests.get(f"http://127.0.0.1:{port}/healthz", timeout=1)
            if response.status_code == 200:
                return
        except requests.RequestException:
            time.sleep(0.25)
    raise RuntimeError("Service did not become ready in time")


def test_index_query_kill(tmp_path: Path):
    env = os.environ.copy()
    kill_path = tmp_path / "brain.KILL"
    index_snapshot = tmp_path / "index.json"

    env.update(
        {
            "SAFE_MODE": "1",
            "LLM_STUB": "1",
            "EMBEDDINGS_BACKEND": "cpu",
            "API_KEY": "devkey",
            "REQUIRE_API_KEY_FOR_WRITES": "1",
            "KILL_PATH": str(kill_path),
            "INDEX_SNAPSHOT": str(index_snapshot),
        }
    )

    process = subprocess.Popen(
        [sys.executable, "-m", "uvicorn", "app.app:app", "--port", str(PORT)],
        cwd="brain-ai-rest-service",
        env=env,
    )
    try:
        wait_ready(PORT)

        payload = {
            "doc_id": "A",
            "text": "GPUs accelerate deep learning training by parallelizing linear algebra.",
        }
        headers = {"X-API-Key": env["API_KEY"], "Content-Type": "application/json"}
        response = requests.post(
            f"http://127.0.0.1:{PORT}/index",
            json=payload,
            timeout=5,
            headers=headers,
        )
        assert response.status_code == 200, response.text

        query = {"query": "What speeds up deep learning?", "top_k": 3}
        response = requests.post(
            f"http://127.0.0.1:{PORT}/query",
            json=query,
            timeout=5,
        )
        assert response.status_code == 200
        data = response.json()
        assert data["hits"], "expected at least one hit"
        assert "Answer" in data["answer"]
        assert data["model"] == "stub"

        kill_path.touch()
        response = requests.post(
            f"http://127.0.0.1:{PORT}/query",
            json=query,
            timeout=5,
        )
        assert response.status_code == 503
    finally:
        process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
