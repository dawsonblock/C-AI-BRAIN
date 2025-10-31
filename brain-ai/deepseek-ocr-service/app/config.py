"""
Configuration settings for DeepSeek-OCR service
"""
import os
from typing import Optional
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    """Service configuration settings"""
    
    # Model configuration
    model_name: str = "deepseek-ai/DeepSeek-OCR"
    model_path: Optional[str] = None  # Local path if not using HuggingFace
    
    # Resolution and mode
    default_resolution: str = "base"  # tiny, small, base, large, gundam
    default_task: str = "markdown"  # ocr, markdown, figure, reference, describe
    
    # Inference parameters
    max_tokens: int = 8192
    temperature: float = 0.0
    top_p: float = 1.0
    
    # vLLM configuration
    use_vllm: bool = True
    enable_prefix_caching: bool = False
    mm_processor_cache_gb: int = 0
    
    # GPU configuration
    device: str = "cuda:0"
    dtype: str = "bfloat16"  # bfloat16, float16, float32
    use_flash_attention: bool = True
    
    # Server configuration
    host: str = "0.0.0.0"
    port: int = 8000
    workers: int = 1  # GPU limited, typically 1
    timeout: int = 60
    
    # Performance
    batch_size: int = 4
    max_concurrent_requests: int = 4
    
    # Logging
    log_level: str = "INFO"
    
    class Config:
        env_prefix = "DEEPSEEK_OCR_"
        case_sensitive = False


# Prompt templates for different tasks
PROMPT_TEMPLATES = {
    "ocr": "<image>\nFree OCR.",
    "markdown": "<image>\n<|grounding|>Convert the document to markdown.",
    "figure": "<image>\n<|grounding|>Parse the figure and extract key information.",
    "reference": "<image>\n<|ref|>Locate and extract all references in the document.<|/ref|>",
    "describe": "<image>\nProvide a detailed description of this image."
}


# Resolution configurations
RESOLUTION_CONFIGS = {
    "tiny": {"size": 512, "vision_tokens": 64},
    "small": {"size": 640, "vision_tokens": 100},
    "base": {"size": 1024, "vision_tokens": 256},
    "large": {"size": 1280, "vision_tokens": 400},
    "gundam": {"dynamic": True, "base": 640, "large": 1024}
}
