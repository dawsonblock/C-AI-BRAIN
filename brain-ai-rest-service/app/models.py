"""Pydantic models for request/response validation."""

from typing import Optional, Dict, Any, List
from pydantic import BaseModel, Field, field_validator


class HealthResponse(BaseModel):
    """Health check response."""
    status: str = Field(..., description="Service status: healthy, degraded, unhealthy")
    version: str = Field(..., description="Service version")
    service: str = Field(..., description="Service name")
    timestamp: float = Field(..., description="Unix timestamp")
    checks: Optional[Dict[str, str]] = Field(default=None, description="Component health checks")


class IndexRequest(BaseModel):
    """Document indexing request."""
    document_id: str = Field(..., min_length=1, max_length=255, description="Unique document identifier")
    text: str = Field(..., min_length=1, max_length=100_000, description="Document text content")
    metadata: Optional[Dict[str, Any]] = Field(default=None, description="Optional metadata")

    @field_validator("text")
    @classmethod
    def validate_text(cls, v: str) -> str:
        """Validate text is not empty after stripping."""
        if not v.strip():
            raise ValueError("text cannot be empty or whitespace only")
        return v


class IndexResponse(BaseModel):
    """Document indexing response."""
    success: bool = Field(..., description="Whether indexing succeeded")
    document_id: str = Field(..., description="Document identifier")
    message: Optional[str] = Field(default=None, description="Status message")


class QueryRequest(BaseModel):
    """Query request."""
    query: str = Field(..., min_length=1, max_length=10_000, description="Search query")
    top_k: int = Field(default=5, ge=1, le=100, description="Number of results to return")
    filters: Optional[Dict[str, Any]] = Field(default=None, description="Optional filters")


class QueryResult(BaseModel):
    """Single query result."""
    document_id: str
    score: float = Field(..., ge=0.0, le=1.0)
    text: str
    metadata: Optional[Dict[str, Any]] = None


class QueryResponse(BaseModel):
    """Query response."""
    success: bool
    results: List[QueryResult] = Field(default_factory=list)
    query_time_ms: float


class CalculateRequest(BaseModel):
    """Safe calculation request."""
    expression: str = Field(..., min_length=1, max_length=500, description="Math expression")

    @field_validator("expression")
    @classmethod
    def validate_expression(cls, v: str) -> str:
        """Validate expression is safe."""
        if not v.strip():
            raise ValueError("expression cannot be empty")
        # Basic validation - will be further validated by safe_eval
        forbidden = ["import", "exec", "eval", "compile", "__", "open", "file"]
        v_lower = v.lower()
        for word in forbidden:
            if word in v_lower:
                raise ValueError(f"forbidden keyword: {word}")
        return v


class CalculateResponse(BaseModel):
    """Calculation response."""
    success: bool
    result: Optional[float] = None
    expression: str
    error: Optional[str] = None


class ErrorResponse(BaseModel):
    """Standard error response."""
    error: str = Field(..., description="Error message")
    detail: Optional[str] = Field(default=None, description="Detailed error information")
    request_id: Optional[str] = Field(default=None, description="Request ID for tracking")
