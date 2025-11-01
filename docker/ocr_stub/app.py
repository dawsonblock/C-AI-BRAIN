from __future__ import annotations

import time
from typing import Dict

from fastapi import FastAPI, UploadFile


app = FastAPI(title="OCR Stub")


@app.post("/ocr")
async def ocr_endpoint(file: UploadFile) -> Dict[str, object]:
    started = time.perf_counter()
    await file.read()  # consume input
    latency = int((time.perf_counter() - started) * 1000)
    return {
        "status": "ok",
        "text": "stubbed OCR text",
        "latency_ms": latency,
    }
