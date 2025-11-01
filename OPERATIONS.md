# Operations Runbook

## Service Endpoints

| Endpoint | Purpose |
|----------|---------|
| `/healthz` | Liveness probe |
| `/readyz` | Dependency readiness (fails when kill switch engaged) |
| `/metrics` | Prometheus exposition |
| `/index` | Document ingest (requires API key) |
| `/query` | Retrieval + LLM stub generation |
| `/facts` | Store/retrieve high confidence answers (requires API key) |
| `/admin/kill` | Toggle kill switch (requires API key) |

## Environment Toggles

- `SAFE_MODE`
  - `1` (default): offline-only, stubbed LLM, external embedding disabled
  - `0`: allow outbound LLM/external embedding calls (requires keys)
- `API_KEY`: rotate via secret store; restart service with updated env
- `INDEX_SNAPSHOT`: change to point at persistent volume if desired
- `KILL_PATH`: set to shared volume for cluster-wide kill switch

## Common Procedures

### Rotate API Key
1. Generate new secret and update deployment environment (env var or secret store).
2. Restart REST service with new `API_KEY`.
3. Distribute key to authorized writers; verify `/index` with new header.

### Toggle SAFE_MODE
1. Set `SAFE_MODE=0` only after ensuring outbound network access is sanctioned.
2. Provide `DEEPSEEK_API_KEY` and (optionally) configure external embedding backend.
3. Restart REST service; monitor logs for outbound call failures.

### Scaling Guidance

- REST service scales horizontally behind a load balancer; ensure sticky kill switch path (`KILL_PATH`) resides on shared storage.
- C++ core can be packaged as a separate service; the REST container already bundles the pybind module for in-process calls. For high QPS, preload the index using `bridge.load_index` and pin workers to CPU cores.
- OCR stub is lightweight; replace with production OCR service as needed.

### Recovery / Kill Switch

1. To halt traffic instantly, touch the kill switch file (`KILL_PATH`).
2. Confirm `/readyz` returns HTTP 503.
3. Remove the file to resume; `/readyz` returns 200 once resumed.

### Observability

- Scrape `/metrics` for request/latency/error counters.
- Logs are structured JSON; forward to ELK or Loki via stdout collector.
- Benchmarks: run `bench/run_bench.py` regularly and archive `bench/results/` + `bench/SUMMARY.md`.

## CI / CD Hooks

- GitHub Actions workflow `ci` runs on pushes/PRs:
  1. Build C++ core (`cmake`, `ctest`)
  2. Python lint (`ruff`, `mypy`)
  3. Python unit tests (`pytest`)
  4. Docker Compose e2e (`pytest tests/e2e`)
  5. Security checks (`bandit`, CodeQL)
- Artefacts: build logs, coverage reports, benchmark CSV, index snapshot.

## Incident Response Checklist

1. Enable kill switch (`touch $KILL_PATH`) to stop write traffic.
2. Capture logs and Prometheus snapshots.
3. Validate API key integrity and rate limiter metrics.
4. Roll back to previous known-good build if necessary.
5. Document the incident (time, impact, mitigation) and create a follow-up issue.
