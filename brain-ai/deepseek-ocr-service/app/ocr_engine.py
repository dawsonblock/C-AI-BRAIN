"""
DeepSeek-OCR Engine
Handles model loading and inference
"""
import time
import logging
from typing import Dict, Any, Optional
from PIL import Image

from app.config import Settings, PROMPT_TEMPLATES

logger = logging.getLogger(__name__)


class OCREngine:
    """OCR engine using DeepSeek-OCR model"""
    
    def __init__(self, settings: Settings):
        """
        Initialize OCR engine
        
        Args:
            settings: Configuration settings
        """
        self.settings = settings
        self.start_time = time.time()
        self.model = None
        self.tokenizer = None
        self.llm = None
        
        self._load_model()
    
    def _load_model(self):
        """Load DeepSeek-OCR model"""
        try:
            if self.settings.use_vllm:
                self._load_vllm_model()
            else:
                self._load_transformers_model()
        except Exception as e:
            logger.error(f"Failed to load model: {e}", exc_info=True)
            raise
    
    def _load_vllm_model(self):
        """Load model using vLLM (recommended for production)"""
        try:
            from vllm import LLM, SamplingParams
            from vllm.sampling_params import NGramPerReqLogitsProcessor
            
            logger.info(f"Loading vLLM model: {self.settings.model_name}")
            
            self.llm = LLM(
                model=self.settings.model_name,
                trust_remote_code=True,
                dtype=self.settings.dtype,
                enable_prefix_caching=self.settings.enable_prefix_caching,
                mm_processor_cache_gb=self.settings.mm_processor_cache_gb,
                gpu_memory_utilization=0.9,
                max_model_len=self.settings.max_tokens
            )
            
            logger.info("vLLM model loaded successfully")
            
        except ImportError:
            logger.warning("vLLM not installed, falling back to transformers")
            self.settings.use_vllm = False
            self._load_transformers_model()
        except Exception as e:
            logger.error(f"Failed to load vLLM model: {e}")
            raise
    
    def _load_transformers_model(self):
        """Load model using HuggingFace Transformers (fallback)"""
        try:
            from transformers import AutoTokenizer, AutoModel
            import torch
            
            logger.info(f"Loading Transformers model: {self.settings.model_name}")
            
            self.tokenizer = AutoTokenizer.from_pretrained(
                self.settings.model_name,
                trust_remote_code=True
            )
            
            self.model = AutoModel.from_pretrained(
                self.settings.model_name,
                trust_remote_code=True,
                use_safetensors=True,
                _attn_implementation='flash_attention_2' if self.settings.use_flash_attention else 'eager',
                torch_dtype=torch.bfloat16 if self.settings.dtype == "bfloat16" else torch.float16
            )
            
            self.model = self.model.eval().cuda().to(
                torch.bfloat16 if self.settings.dtype == "bfloat16" else torch.float16
            )
            
            logger.info("Transformers model loaded successfully")
            
        except ImportError as e:
            logger.error(f"Required dependencies not installed: {e}")
            raise
        except Exception as e:
            logger.error(f"Failed to load Transformers model: {e}")
            raise
    
    def process(
        self,
        image: Image.Image,
        mode: str = "base",
        task: str = "markdown",
        max_tokens: int = 8192,
        temperature: float = 0.0
    ) -> Dict[str, Any]:
        """
        Process image with OCR
        
        Args:
            image: PIL Image
            mode: Resolution mode
            task: Task type
            max_tokens: Maximum tokens to generate
            temperature: Sampling temperature
        
        Returns:
            Dictionary with extracted text and metadata
        """
        try:
            # Get prompt template
            prompt = PROMPT_TEMPLATES.get(task, PROMPT_TEMPLATES["ocr"])
            
            if self.settings.use_vllm:
                return self._process_vllm(image, prompt, max_tokens, temperature)
            else:
                return self._process_transformers(image, prompt, mode, max_tokens, temperature)
                
        except Exception as e:
            logger.error(f"Processing failed: {e}", exc_info=True)
            raise
    
    def _process_vllm(
        self,
        image: Image.Image,
        prompt: str,
        max_tokens: int,
        temperature: float
    ) -> Dict[str, Any]:
        """Process using vLLM"""
        from vllm import SamplingParams
        
        # Prepare input
        model_input = [{
            "prompt": prompt,
            "multi_modal_data": {"image": image}
        }]
        
        # Configure sampling
        sampling_params = SamplingParams(
            temperature=temperature,
            max_tokens=max_tokens,
            top_p=self.settings.top_p
        )
        
        # Generate
        outputs = self.llm.generate(model_input, sampling_params)
        
        # Extract result
        text = outputs[0].outputs[0].text
        tokens_generated = len(outputs[0].outputs[0].token_ids)
        
        return {
            "text": text,
            "confidence": 1.0,  # vLLM doesn't provide confidence scores directly
            "tokens_generated": tokens_generated
        }
    
    def _process_transformers(
        self,
        image: Image.Image,
        prompt: str,
        mode: str,
        max_tokens: int,
        temperature: float
    ) -> Dict[str, Any]:
        """Process using Transformers"""
        import torch
        
        # Prepare inputs
        inputs = self.tokenizer(
            prompt,
            return_tensors="pt",
            padding=True
        ).to(self.model.device)
        
        # Add image
        pixel_values = self.model.process_images([image]).to(
            self.model.device,
            dtype=torch.bfloat16 if self.settings.dtype == "bfloat16" else torch.float16
        )
        
        # Generate
        with torch.no_grad():
            outputs = self.model.generate(
                **inputs,
                pixel_values=pixel_values,
                max_new_tokens=max_tokens,
                temperature=temperature if temperature > 0 else 1.0,
                do_sample=temperature > 0,
                top_p=self.settings.top_p
            )
        
        # Decode
        text = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
        
        # Remove prompt from output
        if text.startswith(prompt):
            text = text[len(prompt):].strip()
        
        return {
            "text": text,
            "confidence": 1.0,
            "tokens_generated": len(outputs[0])
        }
    
    def is_loaded(self) -> bool:
        """Check if model is loaded"""
        if self.settings.use_vllm:
            return self.llm is not None
        else:
            return self.model is not None and self.tokenizer is not None
    
    def cleanup(self):
        """Cleanup resources"""
        logger.info("Cleaning up OCR engine resources")
        if self.model:
            del self.model
        if self.tokenizer:
            del self.tokenizer
        if self.llm:
            del self.llm
        
        # Force garbage collection
        import gc
        gc.collect()
        
        # Clear CUDA cache if available
        try:
            import torch
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
        except ImportError:
            pass
