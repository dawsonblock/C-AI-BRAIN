import os
from pathlib import Path

import pytest
import requests


SAMPLE_IMAGE = Path(__file__).resolve().parent.parent / "data" / "sample.png"


@pytest.mark.integration
def test_ocr_contract():
    url = os.getenv("OCR_URL", "http://127.0.0.1:6001/ocr")
    if not SAMPLE_IMAGE.exists():
        pytest.skip("sample image missing")

    try:
        with SAMPLE_IMAGE.open("rb") as handle:
            response = requests.post(
                url,
                files={"file": ("sample.png", handle, "image/png")},
                timeout=5,
            )
    except requests.ConnectionError:
        pytest.skip("OCR service unavailable")

    assert response.status_code == 200, response.text
    payload = response.json()
    assert isinstance(payload.get("text"), str)
    assert "latency_ms" in payload
    assert "status" in payload
