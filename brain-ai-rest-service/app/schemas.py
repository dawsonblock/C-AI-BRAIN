"""Pydantic schemas for request validation."""

from __future__ import annotations

from typing import List

from pydantic import BaseModel, Field, ConfigDict, field_validator

from .config import settings


class StrictModel(BaseModel):
    model_config = ConfigDict(extra="forbid", str_strip_whitespace=True)


class IndexPayload(StrictModel):
    doc_id: str = Field(min_length=1, max_length=256)
    text: str = Field(min_length=1)

    @field_validator("text")
    @classmethod
    def check_size(cls, value: str) -> str:
        if len(value.encode("utf-8")) > settings.max_doc_bytes:
            raise ValueError("document exceeds MAX_DOC_BYTES")
        return value


class QueryPayload(StrictModel):
    query: str = Field(min_length=1)
    top_k: int = Field(default=5, ge=1, le=20)

    @field_validator("query")
    @classmethod
    def check_tokens(cls, value: str) -> str:
        tokens = value.split()
        if len(tokens) > settings.max_query_tokens:
            raise ValueError("query exceeds MAX_QUERY_TOKENS")
        return value


class FactPayload(StrictModel):
    question: str = Field(min_length=1)
    answer: str = Field(min_length=1)
    confidence: float = Field(ge=0.0, le=1.0)
    citations: List[str] = Field(default_factory=list)


class QueryResponse(BaseModel):
    answer: str
    hits: List[dict]
    model: str
    latency_ms: int


__all__ = [
    "IndexPayload",
    "QueryPayload",
    "FactPayload",
    "QueryResponse",
]
