# Security Overview

Brain-AI ships with conservative defaults so the offline demo is safe by default. Key controls:

## Access Controls

- `API_KEY` is required for all write or admin routes (`/index`, `/facts`, `/admin/*`). Requests without the header are rejected with 401/403.
- API keys are compared exactly; empty or placeholder values are rejected. When `REQUIRE_API_KEY_FOR_WRITES=1` and no key is configured, the service fails closed (503 on write routes).
- Kill switch: touching the file pointed to by `KILL_PATH` (defaults to `/tmp/brain.KILL`) causes all routes to return HTTP 503 until the file is removed.

## Rate Limiting & Quotas

- Per-IP rate limiter (`RATE_LIMIT_RPM`, default 120) enforced in-process.
- Payload guard rails: `MAX_DOC_BYTES` (default 200 KB) and `MAX_QUERY_TOKENS` (default 256) prevent oversized documents or runaway queries.

## SAFE_MODE Defaults

- `SAFE_MODE=1` blocks outbound LLM/embedding calls, forcing deterministic local stubs.
- External embedding backend can only be enabled when SAFE_MODE is disabled, reducing data exfiltration risk.

## Observability & Audit

- JSON logs include timestamp, route, client IP, status code, and latency.
- Prometheus metrics expose request counters, latency histograms, error counts, OCR usage, and index size with bounded label sets.

## Dependency Hygiene

- PyPI dependencies are pinned in `requirements.txt`.
- C++ third parties are vendored/pinned via CMake `FetchContent`.
- Docker images are built on `ubuntu:22.04` / `python:3.11-slim` with non-root users.

## Recommended Hardening Steps

- Rotate `API_KEY` regularly; never check it into source control.
- Run the REST service behind a TLS-terminating proxy or service mesh when moving beyond offline demo mode.
- Enable `SAFE_MODE=0` only when outbound network access is tightly controlled and audited.
- Integrate container image scanning (e.g., Trivy) and periodic CodeQL/Bandit scans (already wired into CI).

Report vulnerabilities responsibly via the repository issue tracker or security contact.
