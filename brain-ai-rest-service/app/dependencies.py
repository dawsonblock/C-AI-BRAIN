"""FastAPI dependencies for authentication and validation."""

from fastapi import Header, HTTPException, status, Request
from typing import Optional
import logging

from .config import settings

logger = logging.getLogger(__name__)


async def require_api_key(
    request: Request,
    x_api_key: Optional[str] = Header(None, alias="X-API-Key")
) -> None:
    """
    Validate API key from request headers.
    
    Args:
        request: FastAPI request object
        x_api_key: API key from X-API-Key header
        
    Raises:
        HTTPException: 401 if API key is missing or invalid
    """
    # Skip validation if no API key is configured (dev only)
    if not settings.api_key:
        if settings.environment == "production":
            logger.error("API key not configured in production")
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="Server configuration error"
            )
        logger.warning("API key validation disabled - development mode")
        return
    
    if not x_api_key:
        logger.warning(
            "Missing API key",
            extra={
                "request_id": getattr(request.state, "request_id", "unknown"),
                "path": request.url.path,
                "client": request.client.host if request.client else "unknown"
            }
        )
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Missing API key",
            headers={"WWW-Authenticate": "ApiKey"}
        )
    
    if x_api_key != settings.api_key:
        logger.warning(
            "Invalid API key",
            extra={
                "request_id": getattr(request.state, "request_id", "unknown"),
                "path": request.url.path,
                "client": request.client.host if request.client else "unknown"
            }
        )
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid API key",
            headers={"WWW-Authenticate": "ApiKey"}
        )
    
    logger.debug(
        "API key validated",
        extra={"request_id": getattr(request.state, "request_id", "unknown")}
    )


async def validate_request_size(request: Request) -> None:
    """
    Validate request body size is within limits.
    
    Args:
        request: FastAPI request object
        
    Raises:
        HTTPException: 413 if payload is too large
    """
    content_length = request.headers.get("content-length")
    if content_length and int(content_length) > settings.max_request_size:
        logger.warning(
            f"Request too large: {content_length} bytes",
            extra={
                "request_id": getattr(request.state, "request_id", "unknown"),
                "content_length": content_length,
                "max_allowed": settings.max_request_size
            }
        )
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail=f"Request body too large. Maximum: {settings.max_request_size} bytes"
        )
