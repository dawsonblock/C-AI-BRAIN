#!/bin/bash
#
# Production startup script for Brain-AI RAG++ system
# Validates environment, builds C++ core, and starts services
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}✓${NC} $1"
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

error() {
    echo -e "${RED}✗${NC} $1"
    exit 1
}

# Step 1: Load environment variables
if [ ! -f .env.production ]; then
    warn ".env.production not found, using template..."
    if [ ! -f .env.production.template ]; then
        error ".env.production.template not found"
    fi
    cp .env.production.template .env.production
    warn "Please edit .env.production with your API keys before continuing!"
    error "Cannot proceed without API keys"
fi

source .env.production
info "Environment variables loaded"

# Step 2: Validate required API keys
if [ -z "$DEEPSEEK_API_KEY" ] || [ "$DEEPSEEK_API_KEY" == "sk-your-deepseek-api-key-here" ]; then
    error "DEEPSEEK_API_KEY not set in .env.production"
fi

if [ -z "$BRAIN_AI_API_KEY" ] || [ "$BRAIN_AI_API_KEY" == "X4Pf6w7C-nagTfwqX_EyERa-nlegzEdUPQV06BC36qU" ]; then
    warn "Using default BRAIN_AI_API_KEY - generate a new one for production!"
    warn "Run: python3 -c \"import secrets; print(secrets.token_urlsafe(32))\""
fi

info "API keys validated"

# Step 3: Build C++ core
info "Building C++ core..."
cd brain-ai
mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_PYTHON_BINDINGS=ON \
      -DBUILD_TESTS=ON \
      ..

make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

if [ $? -ne 0 ]; then
    error "C++ build failed"
fi

info "C++ core built successfully"

# Step 4: Run C++ tests
info "Running C++ tests..."
if ! ctest --output-on-failure; then
    error "C++ tests failed - refusing to start with failing tests"
fi
info "C++ tests passed"

cd ../..

# Step 5: Check Python bindings
if [ ! -f brain-ai/build/brain_ai_py*.so ]; then
    error "Python bindings not found. Build with -DBUILD_PYTHON_BINDINGS=ON"
fi

# Copy bindings to REST service directory
cp brain-ai/build/brain_ai_py*.so brain-ai-rest-service/
info "Python bindings copied"

# Step 6: Install Python dependencies
info "Installing Python dependencies..."
cd brain-ai-rest-service
pip3 install -r requirements.txt -q

if [ $? -ne 0 ]; then
    error "Python dependency installation failed"
fi
info "Python dependencies installed"

cd ..

# Step 7: Create data directories
mkdir -p data/persistence
mkdir -p eval_results
info "Data directories created"

# Step 8: Start services
info "Starting Brain-AI REST Service..."

# Export environment for service
export DEEPSEEK_API_KEY
export BRAIN_AI_API_KEY
export PYTHONUNBUFFERED=1

cd brain-ai-rest-service

# Start with uvicorn
uvicorn app:app \
    --host 0.0.0.0 \
    --port 5001 \
    --timeout-keep-alive 60 \
    --workers 1 \
    --log-level info \
    &

SERVICE_PID=$!

cd ..

info "Brain-AI REST Service started (PID: $SERVICE_PID)"
info "Service URL: http://localhost:5001"
info "Metrics URL: http://localhost:5001/metrics"
info "Health check: curl -H \"X-API-Key: $BRAIN_AI_API_KEY\" http://localhost:5001/health"

# Wait for service to be ready
info "Waiting for service to be ready..."
for i in {1..30}; do
    if curl -s -H "X-API-Key: $BRAIN_AI_API_KEY" http://localhost:5001/health > /dev/null 2>&1; then
        info "Service is ready!"
        break
    fi
    sleep 1
done

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║          Brain-AI RAG++ Production System                  ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "  Status: ${GREEN}RUNNING${NC}"
echo "  PID: $SERVICE_PID"
echo "  Port: 5001"
echo ""
echo "  Features enabled:"
echo "    • C++ vector search backend"
echo "    • Sentence-transformers embeddings (mpnet-base-v2, 768-dim)"
echo "    • DeepSeek LLM (reasoning + chat models)"
echo "    • Multi-agent orchestration (3 solvers + verifier)"
echo "    • Cross-encoder re-ranker"
echo "    • Prometheus metrics"
echo "    • API key authentication"
echo "    • Auto-save persistence (every 10 docs)"
echo "    • Rate limiting (60 req/min)"
echo ""
echo "  Endpoints:"
echo "    • Health: GET /health"
echo "    • Index: POST /api/v1/index_with_text"
echo "    • Query: POST /api/v1/answer"
echo "    • Metrics: GET /metrics"
echo "    • Status: GET /api/v1/system/status"
echo ""
echo "  Authentication:"
echo "    • Add header: X-API-Key: $BRAIN_AI_API_KEY"
echo ""
echo "  To stop:"
echo "    • kill $SERVICE_PID"
echo ""
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Keep script running
wait $SERVICE_PID

