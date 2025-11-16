.PHONY: help build up down logs test lint security clean

help: ## Show this help message
	@echo 'Usage: make [target]'
	@echo ''
	@echo 'Available targets:'
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

build: ## Build Docker images
	docker compose build

build-multi-arch: ## Build multi-arch Docker images (amd64, arm64)
	@echo "Building multi-arch images (linux/amd64,linux/arm64)"
	docker buildx build --platform linux/amd64,linux/arm64 -t brain-ai-rest-service:latest ./brain-ai-rest-service
	docker buildx build --platform linux/amd64,linux/arm64 -t deepseek-ocr-service:latest ./deepseek-ocr-service

sbom: ## Generate SBOMs for built images (requires docker sbom plugin)
	@echo "Generating SBOM for brain-ai-rest-service:latest"
	docker sbom brain-ai-rest-service:latest > sbom-brain-ai-rest.spdx || true
	@echo "Generating SBOM for deepseek-ocr-service:latest"
	docker sbom deepseek-ocr-service:latest > sbom-deepseek-ocr.spdx || true

up: ## Start all services
	docker compose up -d

down: ## Stop all services
	docker compose down

restart: down up ## Restart all services

logs: ## View logs from all services
	docker compose logs -f

logs-rest: ## View logs from REST service
	docker compose logs -f brain-ai-rest

logs-ocr: ## View logs from OCR service
	docker compose logs -f deepseek-ocr

health: ## Check health of all services
	@echo "Checking REST service..."
	@curl -f http://localhost:8000/health || echo "REST service not responding"
	@echo "\nChecking OCR service..."
	@curl -f http://localhost:8001/health || echo "OCR service not responding"

test: ## Run all tests
	@echo "Running REST service tests..."
	cd brain-ai-rest-service && pytest tests/ -v --cov
	@echo "\nRunning OCR service tests..."
	cd deepseek-ocr-service && pytest tests/ -v

test-integration: ## Run integration tests
	docker compose up -d
	@sleep 5
	cd tests/integration && pytest test_integration.py -v
	docker compose down

lint: ## Run linting checks
	@echo "Formatting with black..."
	black brain-ai-rest-service/app deepseek-ocr-service --check
	@echo "\nChecking imports..."
	isort brain-ai-rest-service/app deepseek-ocr-service --check-only
	@echo "\nLinting with flake8..."
	flake8 brain-ai-rest-service/app deepseek-ocr-service --max-line-length=100

format: ## Format code with black and isort
	black brain-ai-rest-service/app deepseek-ocr-service
	isort brain-ai-rest-service/app deepseek-ocr-service

security: ## Run security scans
	@echo "Running bandit..."
	bandit -r brain-ai-rest-service/app deepseek-ocr-service || true
	@echo "\nRunning pip-audit..."
	cd brain-ai-rest-service && pip-audit -r requirements.txt || true
	cd deepseek-ocr-service && pip-audit -r requirements.txt || true

clean: ## Clean up generated files
	find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name '*.pyc' -delete
	find . -type f -name '*.pyo' -delete
	find . -type f -name '*.coverage' -delete
	find . -type d -name '*.egg-info' -exec rm -rf {} + 2>/dev/null || true
	find . -type d -name '.pytest_cache' -exec rm -rf {} + 2>/dev/null || true
	find . -type d -name 'htmlcov' -exec rm -rf {} + 2>/dev/null || true

clean-docker: ## Remove Docker containers, volumes, and networks
	docker compose down -v --remove-orphans

dev-rest: ## Run REST service locally (development mode)
	cd brain-ai-rest-service && uvicorn app.app:app --reload --port 8000

dev-ocr: ## Run OCR service locally (development mode)
	cd deepseek-ocr-service && uvicorn app:app --reload --port 8001

install: ## Install development dependencies
	pip install -r brain-ai-rest-service/requirements.txt
	pip install pytest pytest-asyncio pytest-cov httpx
	pip install black isort flake8 mypy bandit pip-audit
	pip install pre-commit

setup: install ## Setup development environment
	pre-commit install
	@echo "Development environment ready!"

metrics: ## View Prometheus metrics
	curl http://localhost:8000/metrics

api-docs: ## Open API documentation
	@echo "Opening API docs at http://localhost:8000/docs"
	@command -v xdg-open > /dev/null && xdg-open http://localhost:8000/docs || \
	command -v open > /dev/null && open http://localhost:8000/docs || \
	echo "Please open http://localhost:8000/docs in your browser"

backup-db: ## Backup SQLite database
	@echo "Creating database backup..."
	docker exec brain-ai-rest sqlite3 /data/brain_ai.db ".backup /data/backup_$(shell date +%Y%m%d_%H%M%S).db"
	@echo "Backup created successfully"

ps: ## Show running containers
	docker compose ps

stats: ## Show container resource usage
	docker stats --no-stream

shell-rest: ## Open shell in REST service container
	docker compose exec brain-ai-rest sh

shell-ocr: ## Open shell in OCR service container
	docker compose exec deepseek-ocr sh
