"""Pytest configuration and fixtures."""

import os
import pytest


# Set test environment
os.environ["ENVIRONMENT"] = "test"
os.environ["API_KEY"] = "test-api-key-for-testing"
os.environ["LOG_LEVEL"] = "DEBUG"
os.environ["METRICS_ENABLED"] = "true"
os.environ["RATE_LIMIT_ENABLED"] = "false"  # Disable rate limiting in tests


@pytest.fixture(scope="session")
def test_settings():
    """Provide test settings."""
    return {
        "environment": "test",
        "api_key": "test-api-key-for-testing",
        "log_level": "DEBUG",
    }


@pytest.fixture(autouse=True)
def reset_settings():
    """Reset settings after each test."""
    yield
    # Cleanup if needed
