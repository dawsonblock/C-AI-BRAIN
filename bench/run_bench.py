#!/usr/bin/env python3
"""Offline benchmark runner for Brain-AI REST service."""

from __future__ import annotations

import csv
import statistics
import time
from pathlib import Path
from typing import Dict, List, Tuple

import requests


REST_URL = "http://127.0.0.1:5001"
API_KEY = "devkey"
DOC_COUNT = 1000
QUERY_COUNT = 100


def ensure_dirs() -> Tuple[Path, Path]:
    results_dir = Path("bench/results")
    results_dir.mkdir(parents=True, exist_ok=True)
    summary_path = Path("bench/SUMMARY.md")
    return results_dir, summary_path


def index_documents() -> None:
    for idx in range(DOC_COUNT):
        payload = {
            "doc_id": f"doc-{idx}",
            "text": f"Document {idx} explains how CPU embeddings enable offline retrieval for topic {idx % 25}.",
        }
        headers = {"X-API-Key": API_KEY, "Content-Type": "application/json"}
        response = requests.post(f"{REST_URL}/index", json=payload, timeout=5, headers=headers)
        response.raise_for_status()


def run_queries() -> List[Dict[str, object]]:
    latencies: List[float] = []
    records: List[Dict[str, object]] = []
    for idx in range(QUERY_COUNT):
        expected_doc = f"doc-{idx}"
        payload = {"query": f"Explain topic {idx % 25}", "top_k": 5}
        started = time.perf_counter()
        response = requests.post(f"{REST_URL}/query", json=payload, timeout=5)
        latency_ms = (time.perf_counter() - started) * 1000
        if response.status_code != 200:
            records.append(
                {
                    "query_id": idx,
                    "latency_ms": latency_ms,
                    "status": response.status_code,
                    "hits": 0,
                    "recall@5": 0,
                }
            )
            latencies.append(latency_ms)
            continue

        data = response.json()
        hits = data.get("hits", []) or []
        hit_ids = [hit.get("doc_id") for hit in hits]
        recall = 1 if expected_doc in hit_ids else 0
        records.append(
            {
                "query_id": idx,
                "latency_ms": latency_ms,
                "status": 200,
                "hits": len(hits),
                "recall@5": recall,
            }
        )
        latencies.append(latency_ms)
    return records


def write_csv(records: List[Dict[str, object]], results_dir: Path) -> Path:
    csv_path = results_dir / "bench_results.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["query_id", "latency_ms", "status", "hits", "recall@5"])
        writer.writeheader()
        writer.writerows(records)
    return csv_path


def write_summary(records: List[Dict[str, object]], summary_path: Path) -> None:
    latencies = [row["latency_ms"] for row in records]
    recall_values = [row["recall@5"] for row in records]

    p50 = statistics.median(latencies) if latencies else 0.0
    p95 = statistics.quantiles(latencies, n=100)[94] if len(latencies) >= 20 else max(latencies or [0.0])
    recall = sum(recall_values) / len(recall_values) if recall_values else 0.0

    summary = f"""# Brain-AI Offline Benchmark

- Documents indexed: {DOC_COUNT}
- Queries executed: {QUERY_COUNT}
- Latency p50: {p50:.2f} ms
- Latency p95: {p95:.2f} ms
- Recall@5: {recall:.2%}

Artifacts:
- `bench/results/bench_results.csv`
"""

    summary_path.write_text(summary, encoding="utf-8")


def main() -> None:
    results_dir, summary_path = ensure_dirs()
    index_documents()
    records = run_queries()
    write_csv(records, results_dir)
    write_summary(records, summary_path)
    print("Benchmark complete. See bench/SUMMARY.md")


if __name__ == "__main__":
    main()
