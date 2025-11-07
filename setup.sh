#!/bin/bash
set -e

echo "=================================="
echo "C-AI-BRAIN Quick Setup"
echo "=================================="
echo ""

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Check prerequisites
echo "Checking prerequisites..."

if ! command -v docker &> /dev/null; then
    echo -e "${RED}ERROR: Docker not found. Please install Docker 20.10+${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Docker found${NC}"

if ! command -v docker compose &> /dev/null; then
    echo -e "${RED}ERROR: Docker Compose not found. Please install Docker Compose 2.0+${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Docker Compose found${NC}"

echo ""

# Create .env if it doesn't exist
if [ ! -f .env ]; then
    echo -e "${YELLOW}Creating .env file...${NC}"
    cp .env.example .env
    
    # Generate API key
    API_KEY=$(openssl rand -hex 32)
    
    # Update .env with generated key
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        sed -i '' "s/API_KEY=.*/API_KEY=${API_KEY}/" .env
    else
        # Linux
        sed -i "s/API_KEY=.*/API_KEY=${API_KEY}/" .env
    fi
    
    echo -e "${GREEN}✓ .env file created with generated API key${NC}"
    echo -e "${YELLOW}Your API Key: ${API_KEY}${NC}"
    echo -e "${YELLOW}Save this key! You'll need it to access the API.${NC}"
    echo ""
else
    echo -e "${GREEN}✓ .env file already exists${NC}"
    echo ""
fi

# Build images
echo -e "${YELLOW}Building Docker images...${NC}"
docker compose build
echo -e "${GREEN}✓ Docker images built${NC}"
echo ""

# Start services
echo -e "${YELLOW}Starting services...${NC}"
docker compose up -d
echo -e "${GREEN}✓ Services started${NC}"
echo ""

# Wait for services to be ready
echo -e "${YELLOW}Waiting for services to be ready...${NC}"
max_attempts=30
attempt=0

while [ $attempt -lt $max_attempts ]; do
    if curl -f -s http://localhost:8000/health > /dev/null 2>&1 && \
       curl -f -s http://localhost:8001/health > /dev/null 2>&1; then
        echo -e "${GREEN}✓ All services are healthy${NC}"
        break
    fi
    
    attempt=$((attempt + 1))
    if [ $attempt -eq $max_attempts ]; then
        echo -e "${RED}ERROR: Services did not become healthy in time${NC}"
        echo "Check logs with: docker compose logs"
        exit 1
    fi
    
    echo -n "."
    sleep 2
done
echo ""

# Display service info
echo ""
echo "=================================="
echo -e "${GREEN}Setup Complete!${NC}"
echo "=================================="
echo ""
echo "Services are running:"
echo "  • Brain AI REST: http://localhost:8000"
echo "  • DeepSeek OCR:  http://localhost:8001"
echo ""
echo "Useful commands:"
echo "  • View logs:     docker compose logs -f"
echo "  • Stop services: docker compose down"
echo "  • Health check:  curl http://localhost:8000/health"
echo "  • Metrics:       curl http://localhost:8000/metrics"
echo ""
echo "API Documentation:"
echo "  • REST API: http://localhost:8000/docs"
echo "  • OCR API:  http://localhost:8001/docs"
echo ""

# Get API key from .env
if [ -f .env ]; then
    API_KEY_FROM_FILE=$(grep "^API_KEY=" .env | cut -d '=' -f2)
    if [ ! -z "$API_KEY_FROM_FILE" ]; then
        echo "Your API Key: ${API_KEY_FROM_FILE}"
        echo ""
        echo "Test the API:"
        echo "  curl -X POST http://localhost:8000/calculate \\"
        echo "    -H \"X-API-Key: ${API_KEY_FROM_FILE}\" \\"
        echo "    -H \"Content-Type: application/json\" \\"
        echo "    -d '{\"expression\": \"sqrt(16) * 2\"}'"
        echo ""
    fi
fi

echo "For more information, see README.md"
echo ""
