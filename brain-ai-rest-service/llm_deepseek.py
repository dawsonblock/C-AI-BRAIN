"""DeepSeek LLM client with routing, retry logic, and JSON schema enforcement"""
import os
import time
import requests
from typing import List, Dict, Optional, Literal, Union
import logging
import json
from dataclasses import dataclass

logger = logging.getLogger(__name__)


@dataclass
class GenerationConfig:
    """Configuration for LLM generation"""
    temperature: float = 0.0
    max_tokens: int = 2048
    top_p: float = 1.0
    frequency_penalty: float = 0.0
    presence_penalty: float = 0.0


class DeepSeekRouter:
    """Intelligently route queries to appropriate DeepSeek model"""
    
    REASONING_MODEL = "deepseek-reasoner"  # R1 - for complex reasoning
    CHAT_MODEL = "deepseek-chat"           # V3/32B - for retrieval/chat
    
    def select_model(
        self,
        query: str,
        context: str = "",
        force_reasoning: bool = False
    ) -> str:
        """
        Select model based on query complexity and context
        
        Rules:
        - Force reasoning if explicitly requested
        - Use reasoning for math, logic, proofs, analysis
        - Use chat for simple retrieval, long context
        - Default to chat for efficiency
        
        Args:
            query: User query
            context: Retrieved context
            force_reasoning: Force use of reasoning model
        
        Returns:
            Model name to use
        """
        if force_reasoning:
            return self.REASONING_MODEL
        
        query_lower = query.lower()
        
        # Reasoning indicators
        reasoning_keywords = [
            "why", "how", "explain", "prove", "derive", 
            "analyze", "compare", "evaluate", "reason"
        ]
        
        math_keywords = [
            "calculate", "compute", "solve", "equation",
            "formula", "theorem", "proof"
        ]
        
        code_keywords = [
            "debug", "fix", "optimize", "refactor",
            "implement", "algorithm"
        ]
        
        # Check for reasoning needs
        needs_reasoning = (
            any(kw in query_lower for kw in reasoning_keywords) or
            any(kw in query_lower for kw in math_keywords) or
            any(kw in query_lower for kw in code_keywords)
        )
        
        if needs_reasoning:
            logger.info(f"🧠 Routing to {self.REASONING_MODEL} (complex reasoning needed)")
            return self.REASONING_MODEL
        
        # Use chat for long context (more efficient)
        if len(context) > 3000:
            logger.info(f"💬 Routing to {self.CHAT_MODEL} (long context)")
            return self.CHAT_MODEL
        
        # Default to chat
        logger.info(f"💬 Routing to {self.CHAT_MODEL} (default)")
        return self.CHAT_MODEL


class DeepSeekClient:
    """
    DeepSeek API client with retry, backoff, and schema enforcement
    
    Features:
    - Automatic model routing
    - Exponential backoff on rate limits (429)
    - JSON schema enforcement for structured outputs
    - Request/response logging
    - Cost tracking
    """
    
    def __init__(
        self,
        api_key: Optional[str] = None,
        base_url: str = "https://api.deepseek.com",
        max_retries: int = 3,
        timeout: int = 60
    ):
        """
        Initialize DeepSeek client
        
        Args:
            api_key: DeepSeek API key (or from DEEPSEEK_API_KEY env var)
            base_url: API base URL
            max_retries: Maximum retry attempts
            timeout: Request timeout in seconds
        """
        self.api_key = api_key or os.getenv("DEEPSEEK_API_KEY")
        if not self.api_key:
            raise ValueError("DEEPSEEK_API_KEY not set")
        
        self.base_url = base_url
        self.max_retries = max_retries
        self.timeout = timeout
        self.router = DeepSeekRouter()
        
        # Usage tracking
        self.total_tokens = 0
        self.total_requests = 0
        
        logger.info(f"✅ DeepSeek client initialized (base_url={base_url})")
    
    def generate(
        self,
        messages: List[Dict[str, str]],
        model: Optional[str] = None,
        config: Optional[GenerationConfig] = None,
        response_format: Optional[Dict] = None
    ) -> Dict:
        """
        Generate completion with automatic retry and backoff
        
        Args:
            messages: List of {role, content} message dicts
            model: Model name (auto-routes if None)
            config: Generation configuration
            response_format: JSON schema for structured output
        
        Returns:
            {
                "content": str | dict,  # dict if JSON response_format
                "model": str,
                "usage": {...}
            }
        """
        config = config or GenerationConfig()
        
        # Auto-route if model not specified
        if model is None:
            query = messages[-1]["content"] if messages else ""
            context = messages[-2]["content"] if len(messages) >= 2 else ""
            model = self.router.select_model(query, context)
        
        # Retry loop with exponential backoff
        for attempt in range(self.max_retries):
            try:
                response = self._call_api(
                    messages, model, config, response_format
                )
                
                self.total_requests += 1
                self.total_tokens += response.get("usage", {}).get("total_tokens", 0)
                
                return response
                
            except requests.exceptions.HTTPError as e:
                if e.response.status_code == 429:
                    # Rate limit - exponential backoff
                    wait_time = 2 ** attempt
                    logger.warning(
                        f"⏸️  Rate limited (429), "
                        f"waiting {wait_time}s (attempt {attempt+1}/{self.max_retries})"
                    )
                    time.sleep(wait_time)
                    continue
                elif e.response.status_code >= 500:
                    # Server error - retry
                    logger.warning(
                        f"⚠️  Server error ({e.response.status_code}), "
                        f"retrying (attempt {attempt+1}/{self.max_retries})"
                    )
                    time.sleep(1)
                    continue
                else:
                    # Client error - don't retry
                    logger.error(f"❌ Client error ({e.response.status_code}): {e}")
                    raise
            
            except Exception as e:
                logger.error(f"❌ Request failed (attempt {attempt+1}): {e}")
                if attempt == self.max_retries - 1:
                    raise
                time.sleep(1)
        
        raise RuntimeError("Max retries exceeded")
    
    def _call_api(
        self,
        messages: List[Dict],
        model: str,
        config: GenerationConfig,
        response_format: Optional[Dict]
    ) -> Dict:
        """Make API call to DeepSeek"""
        headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {self.api_key}"
        }
        
        payload = {
            "model": model,
            "messages": messages,
            "temperature": config.temperature,
            "max_tokens": config.max_tokens,
            "top_p": config.top_p,
            "frequency_penalty": config.frequency_penalty,
            "presence_penalty": config.presence_penalty
        }
        
        # Add response format if specified
        if response_format:
            payload["response_format"] = response_format
        
        response = requests.post(
            f"{self.base_url}/chat/completions",
            headers=headers,
            json=payload,
            timeout=self.timeout
        )
        response.raise_for_status()
        
        result = response.json()
        content = result["choices"][0]["message"]["content"]
        
        # Parse JSON if response_format was specified
        if response_format and response_format.get("type") == "json_schema":
            try:
                content = json.loads(content)
            except json.JSONDecodeError as e:
                logger.warning(f"Failed to parse JSON response: {e}")
                # Return raw content if parsing fails
        
        return {
            "content": content,
            "model": model,
            "usage": result.get("usage", {})
        }
    
    def generate_with_citations(
        self,
        question: str,
        context: str,
        model: Optional[str] = None,
        evidence_threshold: float = 0.7
    ) -> Dict:
        """
        Generate answer with citations and confidence scoring
        
        Args:
            question: User question
            context: Retrieved context
            model: Model to use (auto-routes if None)
            evidence_threshold: Minimum confidence to answer (else refuse)
        
        Returns:
            {
                "answer": str,
                "citations": [str],
                "confidence": float,
                "refuse": bool,
                "reasoning": str  # If reasoning model used
            }
        """
        # JSON schema for structured output
        schema = {
            "type": "json_schema",
            "json_schema": {
                "name": "answer_with_citations",
                "strict": True,
                "schema": {
                    "type": "object",
                    "properties": {
                        "answer": {
                            "type": "string",
                            "description": "The answer to the question"
                        },
                        "citations": {
                            "type": "array",
                            "items": {"type": "string"},
                            "description": "Relevant citations from context"
                        },
                        "confidence": {
                            "type": "number",
                            "minimum": 0,
                            "maximum": 1,
                            "description": "Confidence score 0-1"
                        },
                        "refuse": {
                            "type": "boolean",
                            "description": "True if insufficient evidence to answer"
                        }
                    },
                    "required": ["answer", "citations", "confidence", "refuse"],
                    "additionalProperties": False
                }
            }
        }
        
        # Cite-first prompt
        system_prompt = f"""You are a precise assistant that answers ONLY from provided context.

Rules:
1. Answer ONLY from the context provided
2. Include specific citations (quote exactly)
3. Set confidence based on evidence strength
4. Set refuse=true if evidence is insufficient (confidence < {evidence_threshold})
5. Be concise and accurate

Output must be valid JSON matching the schema."""
        
        messages = [
            {"role": "system", "content": system_prompt},
            {
                "role": "user",
                "content": f"Context:\n{context}\n\nQuestion: {question}\n\nProvide answer in JSON format:"
            }
        ]
        
        result = self.generate(
            messages, 
            model=model, 
            response_format=schema
        )
        
        # Add model info
        response = result["content"]
        response["model_used"] = result["model"]
        
        return response
    
    def get_usage_stats(self) -> Dict:
        """Get usage statistics"""
        return {
            "total_requests": self.total_requests,
            "total_tokens": self.total_tokens,
            "estimated_cost_usd": self.total_tokens * 0.00014 / 1000  # Rough estimate
        }

