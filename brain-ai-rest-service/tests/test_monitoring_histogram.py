import os
import importlib
import sys
import types
import unittest

try:
    from fastapi.testclient import TestClient
except ModuleNotFoundError:  # pragma: no cover - dependency optional in minimal envs
    TestClient = None


# Ensure API key and histogram defaults are set before importing the app module
os.environ.setdefault("BRAIN_AI_API_KEY", "test-key")
os.environ.setdefault("BRAIN_AI_METRICS_HIST_MAX_SAMPLES", "512")


def ensure_stubbed_dependencies():
    if "sentence_transformers" not in sys.modules:
        module = types.ModuleType("sentence_transformers")

        class DummySentenceTransformer:
            def __init__(self, *args, **kwargs):
                self.args = args
                self.kwargs = kwargs

            def encode(self, text, convert_to_numpy=True):  # pragma: no cover - simple stub
                if isinstance(text, str):
                    length = max(len(text), 1)
                else:
                    length = 1
                return [0.0] * length

        class DummyCrossEncoder:
            def __init__(self, *args, **kwargs):
                self.args = args
                self.kwargs = kwargs

            def predict(self, pairs):  # pragma: no cover - stub
                return [0.0 for _ in pairs]

        module.SentenceTransformer = DummySentenceTransformer
        module.CrossEncoder = DummyCrossEncoder
        sys.modules["sentence_transformers"] = module


def load_app_module():
    """Reload the app module to pick up the current environment configuration."""
    app_module = importlib.import_module("app")
    return importlib.reload(app_module)


class HistogramWindowEndpointTests(unittest.TestCase):
    def setUp(self) -> None:
        if TestClient is None:
            self.skipTest("fastapi is required to run monitoring endpoint tests")

        ensure_stubbed_dependencies()
        self.app_module = load_app_module()
        self.client = TestClient(self.app_module.app)
        self.headers = {"X-API-Key": os.environ["BRAIN_AI_API_KEY"]}

    def test_get_histogram_window_returns_default(self):
        response = self.client.get(
            "/api/v1/monitoring/histogram_window",
            headers=self.headers,
        )

        self.assertEqual(response.status_code, 200)
        self.assertEqual(
            response.json()["histogram_max_samples"],
            self.app_module.metrics.histogram_max_samples,
        )

    def test_post_histogram_window_updates_value(self):
        new_size = 2048
        response = self.client.post(
            "/api/v1/monitoring/histogram_window",
            json={"sample_size": new_size},
            headers=self.headers,
        )

        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["histogram_max_samples"], new_size)

        # Confirm the new value is reflected when reading back
        get_response = self.client.get(
            "/api/v1/monitoring/histogram_window",
            headers=self.headers,
        )

        self.assertEqual(get_response.status_code, 200)
        self.assertEqual(get_response.json()["histogram_max_samples"], new_size)

    def test_post_histogram_window_rejects_invalid_value(self):
        response = self.client.post(
            "/api/v1/monitoring/histogram_window",
            json={"sample_size": 0},
            headers=self.headers,
        )

        self.assertEqual(response.status_code, 400)
        self.assertIn("positive", response.json()["detail"])

    def test_post_histogram_window_caps_upper_bound(self):
        response = self.client.post(
            "/api/v1/monitoring/histogram_window",
            json={"sample_size": 200_000},
            headers=self.headers,
        )

        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["histogram_max_samples"], 100_000)


if __name__ == "__main__":
    unittest.main()
