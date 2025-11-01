# Brain-AI Offline Demo Stack

Brain-AI is now packaged as a reproducible, offline-friendly demo that proves core retrieval + grounding behaviour end-to-end. The upgrade includes a CPU embedding fallback, deterministic LLM stub, hardened REST service, pybind bridge to the C++ index, CI automation, and one-command Docker bring-up.

## Status Snapshot

- Offline demo: ✅ (`SAFE_MODE=1`, `LLM_STUB=1`, CPU embeddings)
- Security guardrails: ✅ (API key on writes, rate limits, kill switch, payload caps)
- Observability: ✅ (Prometheus metrics, JSON logs, health/ready endpoints)
- Benchmarks: ✅ (`bench/run_bench.py` + artifacts)
- Docker Compose: ✅ (`core`, `rest`, `ocr`, `seed` services)
- CI workflow: ✅ (`.github/workflows/ci.yml` builds + tests + e2e + security scans)

## Quickstart (Offline Local Run)

```bash
cd brain-ai-rest-service
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

export SAFE_MODE=1 LLM_STUB=1 EMBEDDINGS_BACKEND=cpu
export API_KEY=devkey REQUIRE_API_KEY_FOR_WRITES=1
uvicorn app.app:app --port 5001
```

Seed a document and run a query:

```bash
export API_URL=http://127.0.0.1:5001 API_KEY=devkey
./scripts/seed.sh

curl -s -X POST "$API_URL/query" \
  -H 'Content-Type: application/json' \
  -d '{"query":"What speeds up deep learning?"}' | jq

# Trigger kill switch and confirm 503
touch /tmp/brain.KILL
curl -i -X POST "$API_URL/query" -H 'Content-Type: application/json' -d '{"query":"test"}'
rm /tmp/brain.KILL
```

## Docker Compose Demo

```bash
docker compose up --build

# Wait for seed container to exit, then:
curl -s -X POST http://127.0.0.1:5001/query \
  -H 'Content-Type: application/json' \
  -d '{"query":"Explain topic 3"}' | jq
```

Services:

- `core`: C++ build artifacts (non-root)
- `rest`: FastAPI stack with pybind bridge + security defaults
- `ocr`: FastAPI OCR stub (`/ocr`)
- `seed`: one-shot curl that indexes a sample doc

Volume `./data` stores the index snapshot and kill-switch file.

## Building the C++ Core

```bash
cd brain-ai && mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
ctest --output-on-failure
```

This produces `brain_ai_core` (pybind module) in `build/python/` for the REST service.

## Python Testing

```bash
cd brain-ai-rest-service
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt pytest

pytest tests/unit -q        # (add unit tests as needed)
pytest tests/e2e -q         # spins up uvicorn, checks index → query → kill switch
```

OCR contract test hits `http://127.0.0.1:6001/ocr`; run via Docker Compose or set `OCR_URL` to a live endpoint. The test skips automatically if the service is unreachable.

## Benchmarks

Run the offline benchmark after the REST service is up (with API key):

```bash
python bench/run_bench.py
cat bench/SUMMARY.md
ls bench/results/
```

Outputs include:

- `bench/results/bench_results.csv`: latency + recall@5 per query
- `bench/SUMMARY.md`: p50/p95 latency and aggregate recall

## Prometheus Metrics & Logging

- `/metrics`: Prometheus exposition (bounded labels: `route`, `status`, `component`)
- `/healthz`: liveness; `/readyz`: dependency readiness
- Logs: JSON lines (`ts`, `lvl`, `event`, `route`, `lat_ms`, `status_code`, `client_ip`)

## Security Hardening

- API key required on `/index`, `/facts`, `/admin/*` (reject missing or mismatched key)
- Request limits: `RATE_LIMIT_RPM` (default 120/min per IP)
- Payload caps: `MAX_DOC_BYTES` (default 200 KB) + token guard (`MAX_QUERY_TOKENS`)
- Kill switch: touching `KILL_PATH` (default `/tmp/brain.KILL`) forces 503 for all requests
- SAFE_MODE fail-closed: disables external LLM/embedding calls and enforces stub

## Configuration Summary

| Env Var | Default | Purpose |
|---------|---------|---------|
| `SAFE_MODE` | `1` | Disable external calls, enforce stub |
| `LLM_STUB` | `1` | Deterministic LLM template when no key |
| `EMBEDDINGS_BACKEND` | `cpu` | Select local vs external embedding path |
| `API_KEY` | _required_ | Write-route auth token |
| `REQUIRE_API_KEY_FOR_WRITES` | `1` | Fail-closed enforcement |
| `MAX_DOC_BYTES` | `200000` | Upper bound on document payload size |
| `MAX_QUERY_TOKENS` | `256` | Token-ish guard on queries |
| `RATE_LIMIT_RPM` | `120` | Per-IP rate limit |
| `INDEX_SNAPSHOT` | `./data/index.json` | Persistence target |
| `KILL_PATH` | `/tmp/brain.KILL` | Kill-switch file |

## Continuous Integration

`.github/workflows/ci.yml` covers:

1. `build_core`: cmake + ctest
2. `lint_py`: ruff + mypy (strict)
3. `unit_py`: pytest on Python code
4. `e2e`: docker compose up, run `pytest tests/e2e`
5. `security`: bandit (Python) + CodeQL (C++/Python)

Artifacts: build logs, coverage XML, index snapshot, benchmark CSV.

## Additional Documentation

- `SECURITY.md`: threat model, auth boundaries, rate limits, dependency pinning
- `OPERATIONS.md`: runbooks for key rotation, SAFE_MODE toggles, scaling notes
- `PRODUCTION_READY_SUMMARY.md`: previous delivery overview (legacy)

## Known Limitations

- LLM responses are stubbed unless `DEEPSEEK_API_KEY` is provided and SAFE_MODE disabled.
- CPU embeddings rely on `sentence-transformers` (first run downloads model if not pre-cached). A deterministic RNG fallback covers fully offline environments.
- In-memory store backs the REST API even when the pybind module is unavailable; persistence is best-effort JSON snapshot.

## Next Steps

- `bench/run_bench.py` for reproducible metrics
- Expand unit coverage for new modules (`app/config.py`, `app/core_bridge.py`)
- Optional Postgres toggle for the facts store
- Optional signed run metadata (“diff envelopes”) after each CI e2e run

---

Questions, issues, or ideas? Open a GitHub issue or drop a PR. Safe defaults are enabled out of the box—disable only when you are ready for full-send mode.
