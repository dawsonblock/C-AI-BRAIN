# Brain-AI Build Instructions

Complete guide to building and deploying Brain-AI with pybind11 bindings.

## 🎯 Quick Start (5 minutes)

```bash
# 1. Build C++ library with Python bindings
cd brain-ai
pip install -e .

# 2. Verify bindings
python3 -c "import brain_ai_py; print(brain_ai_py.__version__)"

# 3. Start REST service (with real C++ integration)
cd ../brain-ai-rest-service
pip install -r requirements.txt
python app_real.py
```

## 📋 Prerequisites

### System Requirements
- **OS**: Ubuntu 20.04+, macOS 10.15+, or Windows 10+ (WSL2)
- **CPU**: x86_64, 4+ cores recommended
- **RAM**: 8GB minimum, 16GB recommended
- **Disk**: 2GB for build artifacts

### Software Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3-dev \
    python3-pip \
    libssl-dev \
    pkg-config
```

#### macOS
```bash
brew install cmake python@3.12 openssl
```

#### Python
```bash
pip install --upgrade pip setuptools wheel
```

## 🔨 Building the C++ Library

### Option 1: Build C++ Only (No Python)

```bash
cd brain-ai
mkdir -p build && cd build

# Configure
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PYTHON_BINDINGS=OFF \
  -DBUILD_TESTS=ON

# Build
make -j$(nproc)

# Test
ctest --output-on-failure

# Run demo
./brain_ai_demo
```

**Expected Output**:
```
Starting Brain-AI Demo...
✅ All tests passed
Demo completed successfully
```

### Option 2: Build with Python Bindings (Recommended)

```bash
cd brain-ai

# Install in development mode (creates brain_ai_py.so)
pip install -e .

# Or build wheel for distribution
pip install build
python -m build
pip install dist/brain_ai-4.3.0-*.whl
```

**Expected Output**:
```
Building C++ extension...
✅ brain_ai_py module built successfully
Successfully installed brain-ai-4.3.0
```

### Option 3: Docker Build

```bash
cd brain-ai
docker build -t brain-ai:4.3.0 .
docker run --rm brain-ai:4.3.0
```

**Note**: Docker build now ENFORCES test success (removed `|| true` bypass).

## 🧪 Testing

### C++ Tests
```bash
cd brain-ai/build
ctest --output-on-failure -V

# Specific test
ctest -R test_cognitive_handler --verbose
```

**Expected**: 6/6 tests passing

### OCR Integration Tests
```bash
cd brain-ai/tests/integration/build
ctest --output-on-failure

# With verbose output
ctest -V
```

**Expected**: 10/10 tests passing

### End-to-End Tests
```bash
# Start services first
cd deepseek-ocr-service && uvicorn app:app --port 8000 &
cd brain-ai-rest-service && python app_real.py &

# Run E2E tests
cd /home/user/webapp
chmod +x test_e2e.sh
./test_e2e.sh
```

**Expected**: 12/12 tests passing

### Python Bindings Tests
```bash
cd brain-ai
python -m pytest tests/python/ -v
```

## 🐍 Using Python Bindings

### Basic Usage

```python
import brain_ai_py

# Initialize cognitive handler
handler = brain_ai_py.CognitiveHandler(
    episodic_capacity=128,
    fusion_weights=brain_ai_py.FusionWeights(
        vector=0.5,
        episodic=0.3,
        semantic=0.2
    ),
    embedding_dim=1536
)

# Index a document
embedding = [0.1] * 1536  # Your embedding here
handler.index_document(
    doc_id="doc_001",
    embedding=embedding,
    content="This is a test document",
    metadata={}
)

# Process a query
query_embedding = [0.1] * 1536  # Your query embedding
result = handler.process_query(
    query="What is this about?",
    query_embedding=query_embedding
)

print(f"Response: {result['response']}")
print(f"Confidence: {result['confidence']}")
print(f"Results: {len(result['results'])}")
```

### REST API Integration

```python
# brain-ai-rest-service/app_real.py automatically uses bindings if available
# Check status:
import requests

response = requests.get("http://localhost:5000/api/v1/health")
print(response.json())

# Expected output:
# {
#   "status": "healthy",
#   "bindings_available": true,  # ✅ Real C++ integration
#   "components": {
#     "brain_ai": "operational",
#     "api": "operational"
#   }
# }
```

## 🚀 Deployment

### Local Development

```bash
# Terminal 1: OCR Service
cd deepseek-ocr-service
uvicorn app:app --reload --port 8000

# Terminal 2: REST API (with C++ bindings)
cd brain-ai-rest-service
python app_real.py

# Terminal 3: Test
curl http://localhost:5000/api/v1/health
```

### Docker Compose

```yaml
# docker-compose.yml
version: '3.8'

services:
  ocr-service:
    build: ./deepseek-ocr-service
    ports:
      - "8000:8000"
    
  brain-ai-api:
    build: ./brain-ai-rest-service
    ports:
      - "5000:5000"
    depends_on:
      - ocr-service
    environment:
      - OCR_SERVICE_URL=http://ocr-service:8000
    volumes:
      - ./brain-ai/build:/app/brain-ai/build
```

```bash
docker-compose up --build
```

### Production (Kubernetes)

See `k8s/` directory for complete manifests.

```bash
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/configmap.yaml
kubectl apply -f k8s/brain-ai-deployment.yaml
kubectl apply -f k8s/brain-ai-service.yaml
```

## 🔍 Troubleshooting

### Issue: `brain_ai_py module not found`

**Solution**:
```bash
cd brain-ai
pip install -e .

# Verify installation
python -c "import brain_ai_py; print('✅ Success')"
```

### Issue: `CMake Error: pybind11 not found`

**Solution**:
```bash
pip install pybind11
# OR install system package
sudo apt-get install pybind11-dev
```

### Issue: Tests failing

**Solution**:
```bash
# Clean rebuild
cd brain-ai
rm -rf build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
ctest --output-on-failure -V
```

### Issue: `ImportError: undefined symbol`

**Solution**: Rebuild with `-fPIC`:
```bash
cd brain-ai/build
cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make clean && make -j$(nproc)
```

### Issue: REST API shows `bindings_available: false`

**Check**:
```bash
# 1. Verify bindings built
ls brain-ai/build/brain_ai_py*.so

# 2. Check Python path
python -c "import sys; print(sys.path)"

# 3. Install in editable mode
cd brain-ai && pip install -e .

# 4. Restart API
cd brain-ai-rest-service && python app_real.py
```

## 📊 Build Performance

### Typical Build Times

| Configuration | Time | Output Size |
|---------------|------|-------------|
| C++ Only (Release) | 2-3 min | ~50MB |
| C++ + Python Bindings | 3-4 min | ~55MB |
| Docker Build | 5-7 min | ~500MB |
| Full Test Suite | 1-2 min | N/A |

### Optimization Flags

**Development** (faster builds, debug symbols):
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

**Production** (optimized, smaller binaries):
```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG"
```

**With Sanitizers** (detect bugs):
```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DUSE_SANITIZERS=ON
```

## 🔒 Security Notes

1. **Bindings Safety**: pybind11 provides automatic type conversion and error handling
2. **Thread Safety**: All C++ operations use mutex protection
3. **Memory Management**: Smart pointers and RAII patterns prevent leaks
4. **Input Validation**: Pydantic models validate all REST API inputs

## 📚 Additional Resources

- **Python Bindings Reference**: See `brain-ai/bindings/brain_ai_bindings.cpp`
- **API Documentation**: See `brain-ai-rest-service/README.md`
- **C++ API**: See header files in `brain-ai/include/`
- **Examples**: See `brain-ai/examples/` directory

## ✅ Verification Checklist

After building, verify:

- [ ] C++ library builds without errors
- [ ] All C++ tests pass (6/6)
- [ ] Python bindings import successfully
- [ ] REST API starts with `bindings_available: true`
- [ ] OCR integration tests pass (10/10)
- [ ] End-to-end tests pass (12/12)
- [ ] Demo application runs
- [ ] Performance benchmarks meet targets

## 🆘 Getting Help

- **GitHub Issues**: https://github.com/dawsonblock/C-AI-BRAIN/issues
- **Build Logs**: Check `brain-ai/build/CMakeFiles/CMakeOutput.log`
- **Test Logs**: Run `ctest -V` for verbose output

---

**Last Updated**: October 31, 2025  
**Version**: 4.3.0  
**Status**: Production-Ready with pybind11 Integration
