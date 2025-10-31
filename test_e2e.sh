#!/bin/bash
# End-to-End Test Script for Brain-AI System
# Tests the complete pipeline: OCR Service → REST API → Document Processing → Query

# set -e  # Exit on error (removed to allow all tests to run)

echo "======================================"
echo "Brain-AI End-to-End Integration Test"
echo "======================================"
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Service URLs
OCR_SERVICE="http://localhost:8000"
REST_SERVICE="http://localhost:5000"

# Test counters
TESTS_TOTAL=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
test_start() {
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    echo -n "Test $TESTS_TOTAL: $1 ... "
}

test_pass() {
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo -e "${GREEN}PASS${NC}"
}

test_fail() {
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo -e "${RED}FAIL${NC}"
    echo "  Error: $1"
}

# ==================== Test 1: OCR Service Health ====================
test_start "OCR Service Health Check"
if response=$(curl -s -f "$OCR_SERVICE/health"); then
    status=$(echo "$response" | jq -r '.status')
    if [ "$status" = "healthy" ]; then
        test_pass
    else
        test_fail "OCR service not healthy"
    fi
else
    test_fail "OCR service not responding"
fi

# ==================== Test 2: REST Service Health ====================
test_start "REST Service Health Check"
if response=$(curl -s -f "$REST_SERVICE/api/v1/health"); then
    status=$(echo "$response" | jq -r '.status')
    if [ "$status" = "healthy" ]; then
        test_pass
    else
        test_fail "REST service not healthy"
    fi
else
    test_fail "REST service not responding"
fi

# ==================== Test 3: OCR Text Extraction ====================
test_start "OCR Text Extraction"

# Create test document
TEST_DOC="/tmp/e2e_test_doc.txt"
echo "This is a test document for end-to-end testing. It contains sample text that will be processed by the Brain-AI system through the OCR service, REST API, and document processing pipeline." > "$TEST_DOC"

# Extract text via OCR
if response=$(curl -s -f -X POST "$OCR_SERVICE/ocr/extract" \
    -F "file=@$TEST_DOC" \
    -F "mode=base" \
    -F "task=ocr"); then
    
    success=$(echo "$response" | jq -r '.success')
    text=$(echo "$response" | jq -r '.text')
    
    if [ "$success" = "true" ] && [ -n "$text" ]; then
        test_pass
    else
        test_fail "OCR extraction failed"
    fi
else
    test_fail "OCR request failed"
fi

# ==================== Test 4: Document Processing via REST API ====================
test_start "Document Processing via REST API"

if response=$(curl -s -f -X POST "$REST_SERVICE/api/v1/documents/process" \
    -H "Content-Type: application/json" \
    -d "{
        \"doc_id\": \"e2e_test_001\",
        \"file_path\": \"$TEST_DOC\",
        \"ocr_config\": {
            \"service_url\": \"$OCR_SERVICE\",
            \"mode\": \"base\",
            \"task\": \"ocr\"
        },
        \"create_memory\": true,
        \"index_document\": true
    }"); then
    
    success=$(echo "$response" | jq -r '.success')
    indexed=$(echo "$response" | jq -r '.indexed')
    
    if [ "$success" = "true" ] && [ "$indexed" = "true" ]; then
        test_pass
    else
        test_fail "Document processing failed"
    fi
else
    test_fail "Document processing request failed"
fi

# ==================== Test 5: Batch Document Processing ====================
test_start "Batch Document Processing"

# Create multiple test documents
TEST_DOC1="/tmp/e2e_batch_1.txt"
TEST_DOC2="/tmp/e2e_batch_2.txt"
TEST_DOC3="/tmp/e2e_batch_3.txt"

echo "Batch document 1 content" > "$TEST_DOC1"
echo "Batch document 2 content" > "$TEST_DOC2"
echo "Batch document 3 content" > "$TEST_DOC3"

if response=$(curl -s -f -X POST "$REST_SERVICE/api/v1/documents/batch" \
    -H "Content-Type: application/json" \
    -d "{
        \"documents\": [
            {\"doc_id\": \"batch_001\", \"file_path\": \"$TEST_DOC1\", \"index_document\": true},
            {\"doc_id\": \"batch_002\", \"file_path\": \"$TEST_DOC2\", \"index_document\": true},
            {\"doc_id\": \"batch_003\", \"file_path\": \"$TEST_DOC3\", \"index_document\": true}
        ],
        \"ocr_config\": {
            \"service_url\": \"$OCR_SERVICE\",
            \"mode\": \"base\"
        }
    }"); then
    
    total=$(echo "$response" | jq -r '.total')
    successful=$(echo "$response" | jq -r '.successful')
    
    if [ "$total" = "3" ] && [ "$successful" = "3" ]; then
        test_pass
    else
        test_fail "Batch processing: expected 3/3, got $successful/$total"
    fi
else
    test_fail "Batch processing request failed"
fi

# ==================== Test 6: Query Processing ====================
test_start "Query Processing"

# Mock embedding (in production, this would come from an embedding service)
# Generate a 1536-dimensional vector for testing
MOCK_EMBEDDING=$(python -c "print(str([0.1] * 1536).replace(' ', ''))")

if response=$(curl -s -f -X POST "$REST_SERVICE/api/v1/query" \
    -H "Content-Type: application/json" \
    -d "{
        \"query\": \"What is in the test documents?\",
        \"query_embedding\": $MOCK_EMBEDDING,
        \"top_k\": 5,
        \"use_episodic\": true,
        \"use_semantic\": true
    }"); then
    
    query=$(echo "$response" | jq -r '.query')
    response_text=$(echo "$response" | jq -r '.response')
    confidence=$(echo "$response" | jq -r '.confidence')
    
    if [ -n "$response_text" ] && [ "$confidence" != "null" ]; then
        test_pass
    else
        test_fail "Query processing returned invalid response"
    fi
else
    test_fail "Query processing request failed"
fi

# ==================== Test 7: Vector Search ====================
test_start "Vector Search"

if response=$(curl -s -f -X POST "$REST_SERVICE/api/v1/search" \
    -H "Content-Type: application/json" \
    -d "{
        \"query_embedding\": $MOCK_EMBEDDING,
        \"top_k\": 3,
        \"similarity_threshold\": 0.7
    }"); then
    
    total_results=$(echo "$response" | jq -r '.total_results')
    
    if [ "$total_results" -ge 0 ]; then
        test_pass
    else
        test_fail "Vector search returned invalid results"
    fi
else
    test_fail "Vector search request failed"
fi

# ==================== Test 8: Document Indexing ====================
test_start "Document Indexing"

if response=$(curl -s -f -X POST "$REST_SERVICE/api/v1/index" \
    -H "Content-Type: application/json" \
    -d "{
        \"doc_id\": \"direct_index_001\",
        \"embedding\": $MOCK_EMBEDDING,
        \"content\": \"This document was directly indexed without OCR\",
        \"metadata\": {\"source\": \"test\", \"timestamp\": \"2024-10-31T10:00:00Z\"}
    }"); then
    
    success=$(echo "$response" | jq -r '.success')
    indexed=$(echo "$response" | jq -r '.indexed')
    
    if [ "$success" = "true" ] && [ "$indexed" = "true" ]; then
        test_pass
    else
        test_fail "Document indexing failed"
    fi
else
    test_fail "Document indexing request failed"
fi

# ==================== Test 9: Episode Addition ====================
test_start "Episode Addition"

if response=$(curl -s -f -X POST "$REST_SERVICE/api/v1/episodes" \
    -H "Content-Type: application/json" \
    -d "{
        \"query\": \"Test query for episode\",
        \"response\": \"Test response for episode\",
        \"query_embedding\": $MOCK_EMBEDDING,
        \"metadata\": {\"test\": true}
    }"); then
    
    success=$(echo "$response" | jq -r '.success')
    episode_id=$(echo "$response" | jq -r '.episode_id')
    
    if [ "$success" = "true" ] && [ -n "$episode_id" ] && [ "$episode_id" != "null" ]; then
        test_pass
    else
        test_fail "Episode addition failed"
    fi
else
    test_fail "Episode addition request failed"
fi

# ==================== Test 10: Recent Episodes Retrieval ====================
test_start "Recent Episodes Retrieval"

if response=$(curl -s -f "$REST_SERVICE/api/v1/episodes/recent?limit=5"); then
    total=$(echo "$response" | jq -r '.total')
    
    if [ "$total" -ge 0 ]; then
        test_pass
    else
        test_fail "Recent episodes retrieval failed"
    fi
else
    test_fail "Recent episodes request failed"
fi

# ==================== Test 11: Service Statistics ====================
test_start "Service Statistics"

if response=$(curl -s -f "$REST_SERVICE/api/v1/stats"); then
    total_documents=$(echo "$response" | jq -r '.total_documents')
    total_queries=$(echo "$response" | jq -r '.total_queries')
    
    if [ "$total_documents" -gt 0 ] && [ "$total_queries" -gt 0 ]; then
        test_pass
    else
        test_fail "Statistics show no activity"
    fi
else
    test_fail "Statistics request failed"
fi

# ==================== Test 12: Performance Check ====================
test_start "Performance Check (Query < 500ms)"

START=$(date +%s%N)
curl -s -X POST "$REST_SERVICE/api/v1/query" \
    -H "Content-Type: application/json" \
    -d "{
        \"query\": \"Performance test query\",
        \"query_embedding\": $MOCK_EMBEDDING,
        \"top_k\": 3
    }" > /dev/null
END=$(date +%s%N)

DURATION_MS=$(( (END - START) / 1000000 ))

if [ "$DURATION_MS" -lt 500 ]; then
    test_pass
    echo "  Query completed in ${DURATION_MS}ms"
else
    test_fail "Query took ${DURATION_MS}ms (target: <500ms)"
fi

# ==================== Final Results ====================
echo ""
echo "======================================"
echo "Test Results Summary"
echo "======================================"
echo ""
echo "Total Tests:  $TESTS_TOTAL"
echo -e "Passed:       ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed:       ${RED}$TESTS_FAILED${NC}"
echo ""

if [ "$TESTS_TOTAL" -gt 0 ]; then
    SUCCESS_RATE=$((TESTS_PASSED * 100 / TESTS_TOTAL))
    echo "Success Rate: $SUCCESS_RATE%"
else
    echo "Success Rate: N/A (no tests run)"
fi
echo ""

if [ "$TESTS_FAILED" -eq 0 ]; then
    echo -e "${GREEN}✅ All tests passed!${NC}"
    echo ""
    echo "System Status:"
    echo "  - OCR Service:     Running ✓"
    echo "  - REST Service:    Running ✓"
    echo "  - Document Pipeline: Functional ✓"
    echo "  - Query System:    Functional ✓"
    echo "  - Vector Search:   Functional ✓"
    echo "  - Episodic Memory: Functional ✓"
    echo ""
    exit 0
else
    echo -e "${RED}❌ Some tests failed${NC}"
    echo ""
    echo "Please check the service logs:"
    echo "  - OCR Service:  deepseek-ocr-service/ocr_service.log"
    echo "  - REST Service: brain-ai-rest-service/rest_service.log"
    echo ""
    exit 1
fi
