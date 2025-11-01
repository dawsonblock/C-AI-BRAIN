"""Compatibility module exposing the FastAPI application instance."""

from app.app import app, create_app  # noqa: F401

__all__ = ["app", "create_app"]
