"""Application configuration with secure defaults."""

import os
from typing import List
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """Application settings with environment variable support."""

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore"
    )

    # API Security
    api_key: str = os.getenv("API_KEY", "")
    
    # CORS - Default to empty (no origins allowed) in production
    cors_origins: List[str] = []
    cors_credentials: bool = False
    cors_methods: List[str] = ["GET", "POST"]
    cors_headers: List[str] = ["*"]
    
    # Rate Limiting
    rate_limit_enabled: bool = True
    rate_limit_default: str = "100/minute"
    
    # Request Limits
    max_request_size: int = 1_000_000  # 1MB default for text
    max_text_length: int = 100_000  # 100K chars
    
    # Database
    db_path: str = os.getenv("DB_PATH", "/data/brain_ai.db")
    db_pool_size: int = 5
    db_timeout: int = 5000  # milliseconds
    
    # Embeddings
    embedding_backend: str = os.getenv("EMBEDDING_BACKEND", "cpu")
    embedding_model: str = os.getenv("EMBEDDING_MODEL", "all-MiniLM-L6-v2")
    embedding_dimensions: int = 384
    
    # Metrics
    metrics_enabled: bool = True
    
    # Logging
    log_level: str = os.getenv("LOG_LEVEL", "INFO")
    log_json: bool = True
    
    # Security
    secure_cookies: bool = os.getenv("SECURE_COOKIES", "false").lower() == "true"
    hsts_enabled: bool = False
    
    # Service Info
    service_name: str = "brain-ai-rest-service"
    version: str = "1.0.0"
    environment: str = os.getenv("ENVIRONMENT", "production")

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        # Parse CORS origins from comma-separated string if provided
        cors_env = os.getenv("CORS_ORIGINS", "")
        if cors_env:
            self.cors_origins = [origin.strip() for origin in cors_env.split(",") if origin.strip()]
        # Validate API key is set in production
        if self.environment == "production" and not self.api_key:
            raise ValueError("API_KEY must be set in production environment")


settings = Settings()
