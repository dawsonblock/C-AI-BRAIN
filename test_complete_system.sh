#!/bin/bash
# Comprehensive end-to-end test suite for RAG++ system

set -e  # Exit on error

BASE_URL="http://localhost:5001"
FAILED=0
PASSED=0

echo "========================================="
echo "Brain-AI RAG++ End-to-End Test Suite"
echo "========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    FAILED=$((FAILED + 1))
}

info() {
    echo -e "${YELLOW}→${NC} $1"
}

# Wait for service to be ready
info "Waiting for service to be fully ready..."
MAX_RETRIES=10
RETRY_COUNT=0
while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
    if curl -s "$BASE_URL/api/v1/system/status" > /dev/null 2>&1; then
        # Check if all critical components are loaded
        COMPONENTS=$(curl -s "$BASE_URL/api/v1/system/status" | jq -r '.components.embedding_model')
        if [ "$COMPONENTS" = "true" ]; then
            echo -e "${GREEN}✓${NC} Service is ready"
            break
        fi
    fi
    RETRY_COUNT=$((RETRY_COUNT + 1))
    echo "  Waiting... ($RETRY_COUNT/$MAX_RETRIES)"
    sleep 2
done

if [ $RETRY_COUNT -eq $MAX_RETRIES ]; then
    echo -e "${RED}✗ Service not ready after $MAX_RETRIES attempts${NC}"
    exit 1
fi

sleep 2  # Extra buffer for model warmup

# Test 1: System Status
info "Test 1: System Status"
STATUS=$(curl -s "$BASE_URL/api/v1/system/status")
if echo "$STATUS" | jq -e '.components.cpp_backend == true' > /dev/null; then
    pass "System status endpoint"
else
    fail "System status endpoint"
fi

# Test 2: Index documents (use unique IDs with timestamp)
info "Test 2: Indexing documents"
TIMESTAMP=$(date +%s)
DOC1=$(curl -s -X POST "$BASE_URL/api/v1/index_with_text" \
    -H "Content-Type: application/json" \
    -d "{\"doc_id\":\"test1_${TIMESTAMP}\",\"text\":\"Python is a programming language created by Guido van Rossum.\"}")

# Debug output if needed
if [ -z "$DOC1" ]; then
    fail "Document indexing (empty response)"
elif echo "$DOC1" | jq -e '.success == true' > /dev/null 2>&1; then
    pass "Document indexing"
else
    echo "  Response: $(echo $DOC1 | jq -c '.' 2>/dev/null || echo $DOC1)"
    fail "Document indexing"
fi

# Index more documents with unique IDs
curl -s -X POST "$BASE_URL/api/v1/index_with_text" \
    -H "Content-Type: application/json" \
    -d "{\"doc_id\":\"test2_${TIMESTAMP}\",\"text\":\"The Apollo Guidance Computer used 4KB of RAM and 72KB of rope memory.\"}" > /dev/null

curl -s -X POST "$BASE_URL/api/v1/index_with_text" \
    -H "Content-Type: application/json" \
    -d "{\"doc_id\":\"test3_${TIMESTAMP}\",\"text\":\"Machine learning is a subset of artificial intelligence that uses algorithms to learn from data.\"}" > /dev/null

# Test 3: RAG++ Query (Single-agent)
info "Test 3: RAG++ Query with Citations"
ANSWER=$(curl -s -X POST "$BASE_URL/api/v1/answer" \
    -H "Content-Type: application/json" \
    -d '{"question":"Who created Python?","evidence_threshold":0.7}')

if echo "$ANSWER" | jq -e '.answer' | grep -qi "guido\|rossum"; then
    pass "RAG++ single-agent query"
else
    fail "RAG++ single-agent query"
fi

# Test 4: Multi-agent orchestration
info "Test 4: Multi-agent orchestration"
MULTI_ANSWER=$(curl -s -X POST "$BASE_URL/api/v1/answer" \
    -H "Content-Type: application/json" \
    -d '{"question":"How much RAM did the Apollo computer have?","use_multi_agent":true}')

if echo "$MULTI_ANSWER" | jq -e '.method' | grep -q "judged\|early_stop"; then
    pass "Multi-agent orchestration"
else
    fail "Multi-agent orchestration"
fi

# Test 5: Smart Chunking
info "Test 5: Smart chunking"
LONG_TEXT="Machine learning is a method of data analysis that automates analytical model building. It is a branch of artificial intelligence based on the idea that systems can learn from data, identify patterns and make decisions with minimal human intervention. Through the use of statistical techniques, machine learning algorithms build mathematical models based on sample data, known as training data, in order to make predictions or decisions without being explicitly programmed to perform the task. Machine learning algorithms are used in a wide variety of applications, such as email filtering and computer vision, where it is difficult or infeasible to develop conventional algorithms to perform the needed tasks."
CHUNKS=$(curl -s -X POST "$BASE_URL/api/v1/chunk" \
    -H "Content-Type: application/json" \
    -d "{\"text\":\"$LONG_TEXT\",\"doc_id\":\"chunk_test\"}")

if echo "$CHUNKS" | jq -e '.num_chunks >= 0' > /dev/null; then
    pass "Smart chunking"
else
    fail "Smart chunking"
fi

# Test 6: Facts Store
info "Test 6: Facts store statistics"
FACTS=$(curl -s "$BASE_URL/api/v1/facts/stats")
if echo "$FACTS" | jq -e '.total_facts >= 0' > /dev/null; then
    pass "Facts store"
else
    fail "Facts store"
fi

# Test 7: Persistence Save
info "Test 7: Persistence save"
SAVE=$(curl -s -X POST "$BASE_URL/api/v1/persistence/save")
if echo "$SAVE" | jq -e '.success == true' > /dev/null; then
    pass "Persistence save"
else
    fail "Persistence save"
fi

# Test 8: Persistence Info
info "Test 8: Persistence info"
sleep 1  # Wait for save to complete
STATUS2=$(curl -s "$BASE_URL/api/v1/system/status")
if echo "$STATUS2" | jq -e '.persistence_info' > /dev/null; then
    pass "Persistence info"
else
    fail "Persistence info"
fi

# Test 9: Statistics endpoint
info "Test 9: Statistics endpoint"
STATS=$(curl -s "$BASE_URL/api/v1/stats")
if echo "$STATS" | jq -e '.vector_index_size >= 0' > /dev/null; then
    pass "Statistics endpoint"
else
    fail "Statistics endpoint"
fi

# Test 10: Component verification
info "Test 10: All components active"
COMPONENTS=$(curl -s "$BASE_URL/api/v1/system/status" | jq -c '.components')
if echo "$COMPONENTS" | jq -e '.cpp_backend == true and .embedding_model == true and .reranker == true and .facts_store == true and .deepseek_llm == true and .multi_agent == true and .persistence == true' > /dev/null; then
    pass "All components active"
else
    fail "All components active"
fi

# Summary
echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi

