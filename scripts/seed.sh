#!/usr/bin/env bash
set -euo pipefail

API_URL=${API_URL:-http://127.0.0.1:5001}
API_KEY=${API_KEY:-devkey}

payload='{"doc_id":"A","text":"GPUs accelerate deep learning training by parallelizing linear algebra."}'

curl -s -X POST "$API_URL/index" \
  -H "X-API-Key: $API_KEY" \
  -H 'Content-Type: application/json' \
  -d "$payload"

echo "Seeded sample document." 1>&2
