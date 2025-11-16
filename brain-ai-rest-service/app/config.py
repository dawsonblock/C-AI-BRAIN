"""Application configuration with secure defaults."""

import os
from typing import Dict, List, Set
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
    api_keys_env: str = os.getenv("API_KEYS", "")
    api_key_roles: Dict[str, str] = {}
    allowed_roles: Set[str] = {"admin", "user", "service"}
    default_role: str = "user"
    api_key_session_minutes: int = 60 * 24  # 24 hours

    # JWT / Token Security
    jwt_secret: str = os.getenv("JWT_SECRET", "")
    jwt_algorithm: str = os.getenv("JWT_ALGORITHM", "HS256")
    jwt_issuer: str = os.getenv("JWT_ISSUER", "c-ai-brain")
    jwt_audience: str | None = os.getenv("JWT_AUDIENCE", None)
    jwt_leeway_seconds: int = int(os.getenv("JWT_LEEWAY_SECONDS", "5"))
    jwt_default_ttl_minutes: int = int(os.getenv("JWT_TTL_MINUTES", "60"))
    
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
        # Normalize API key role mappings
        self.api_key_roles = self._load_api_keys()
        if self.api_key and self.api_key not in self.api_key_roles:
            self.api_key_roles[self.api_key] = self.default_role

        # Validate required secrets in production
        if self.environment == "production":
            if not self.jwt_secret:
                raise ValueError("JWT_SECRET must be set in production environment")
            if not self.api_key_roles:
                raise ValueError("At least one API key must be configured in production")

    def _load_api_keys(self) -> Dict[str, str]:
        """Parse API key -> role mappings from environment variable."""
        mapping: Dict[str, str] = {}
        entries = [entry.strip() for entry in self.api_keys_env.split(",") if entry.strip()]
        for entry in entries:
            if ":" not in entry:
                raise ValueError("API_KEYS entries must use 'role:key' format")
            role, key = entry.split(":", 1)
            role = role.strip().lower()
            key = key.strip()
            if role not in self.allowed_roles:
                raise ValueError(f"Unsupported role '{role}' in API_KEYS")
            if not key:
                raise ValueError("API_KEYS entries must include a key value")
            mapping[key] = role
        return mapping


settings = Settings()
