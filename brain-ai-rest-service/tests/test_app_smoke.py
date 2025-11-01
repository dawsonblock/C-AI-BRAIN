import os
from importlib import reload
from pathlib import Path

from fastapi.testclient import TestClient


def build_client(tmp_path: Path) -> TestClient:
    os.environ["SAFE_MODE"] = "1"
    os.environ["LLM_STUB"] = "1"
    os.environ["EMBEDDINGS_BACKEND"] = "cpu"
    os.environ["API_KEY"] = "test-key"
    os.environ["REQUIRE_API_KEY_FOR_WRITES"] = "1"
    os.environ["INDEX_SNAPSHOT"] = str(tmp_path / "index.json")
    os.environ["KILL_PATH"] = str(tmp_path / "kill")

    import app.app as app_module

    reload(app_module)
    return TestClient(app_module.app)


def test_health_ready(tmp_path):
    client = build_client(tmp_path)
    assert client.get("/healthz").status_code == 200
    assert client.get("/readyz").status_code == 200


def test_index_requires_api_key(tmp_path):
    client = build_client(tmp_path)
    payload = {"doc_id": "doc-1", "text": "Example offline document."}
    response = client.post("/index", json=payload)
    assert response.status_code == 401

    response = client.post("/index", json=payload, headers={"X-API-Key": "test-key"})
    assert response.status_code == 200


def test_kill_switch_returns_503(tmp_path):
    client = build_client(tmp_path)
    kill_path = Path(os.environ["KILL_PATH"])
    kill_path.touch()

    response = client.post("/query", json={"query": "test"})
    assert response.status_code == 503

    kill_path.unlink()
    response = client.post("/query", json={"query": "test"})
    assert response.status_code == 200
