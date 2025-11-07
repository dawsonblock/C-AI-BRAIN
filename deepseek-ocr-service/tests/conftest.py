"""Pytest configuration for OCR service."""

import os
import pytest


# Set test environment
os.environ["LOG_LEVEL"] = "DEBUG"


@pytest.fixture(scope="session")
def test_settings():
    """Provide test settings."""
    return {
        "log_level": "DEBUG",
        "max_file_size": 50 * 1024 * 1024,
    }
