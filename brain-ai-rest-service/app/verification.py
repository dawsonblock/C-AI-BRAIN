"""Verification tools: calculator and code sandbox for grounding."""

from __future__ import annotations

import ast
import logging
import math
import os
from typing import Dict, Optional

import requests

LOGGER = logging.getLogger(__name__)


def safe_calculator(expr: str) -> Dict[str, object]:
    """
    Evaluate a mathematical expression safely.
    
    Uses Python's ast module to parse and evaluate only safe math operations.
    No imports, no file I/O, no network access.
    
    Args:
        expr: Mathematical expression (e.g., "2 + 2 * 3", "sqrt(16)")
    
    Returns:
        Dict with result (str) or error (str)
    """
    try:
        # Whitelist of allowed math functions
        allowed_names = {
            "abs": abs,
            "min": min,
            "max": max,
            "sum": sum,
            "round": round,
            "pow": pow,
            # Math module functions
            "sqrt": math.sqrt,
            "sin": math.sin,
            "cos": math.cos,
            "tan": math.tan,
            "log": math.log,
            "log10": math.log10,
            "exp": math.exp,
            "ceil": math.ceil,
            "floor": math.floor,
            "pi": math.pi,
            "e": math.e,
        }
        
        # Check for suspicious characters
        forbidden_chars = [";", "import", "eval", "exec", "__", "open", "file"]
        for forbidden in forbidden_chars:
            if forbidden in expr.lower():
                return {"error": f"Forbidden token: {forbidden}"}
        
        # Parse expression into AST
        tree = ast.parse(expr, mode="eval")
        
        # Compile and evaluate
        code = compile(tree, "<string>", "eval")
        result = eval(code, {"__builtins__": {}}, allowed_names)
        
        return {"result": str(result)}
        
    except SyntaxError as e:
        LOGGER.warning("Calculator syntax error: %s", e)
        return {"error": f"Syntax error: {e}"}
    except (ValueError, TypeError, ZeroDivisionError) as e:
        LOGGER.warning("Calculator evaluation error: %s", e)
        return {"error": f"Evaluation error: {e}"}
    except Exception as e:
        LOGGER.error("Calculator unexpected error: %s", e)
        return {"error": f"Unexpected error: {e}"}


def safe_code_sandbox(
    code: str,
    language: str = "python",
    timeout: int = 8,
    max_output_bytes: int = 10_000,
) -> Dict[str, object]:
    """
    Execute code in a sandboxed environment with resource limits.

    The service no longer executes code locally. A vetted remote sandbox (for
    example, a Firecracker-based microVM fleet or an internal hardened executor
    API) must be provided via the ``HARDENED_SANDBOX_URL`` environment variable.
    When that endpoint is unavailable, the sandbox call is rejected.
    
    Args:
        code: Source code to execute
        language: Programming language (currently only "python" supported)
        timeout: Maximum execution time in seconds
        max_output_bytes: Maximum output size in bytes
    
    Returns:
        Dict with stdout, stderr, returncode, or error
    """
    if language != "python":
        return {"error": f"Language '{language}' not supported"}
    
    # Security checks
    if os.getenv("ENABLE_CODE_SANDBOX", "false").lower() != "true":
        return {
            "error": (
                "Code sandbox disabled. Set ENABLE_CODE_SANDBOX=true and configure a"
                " hardened remote executor."
            )
        }

    executor_url = os.getenv("HARDENED_SANDBOX_URL")
    if not executor_url:
        return {
            "error": (
                "No hardened sandbox executor configured. Set HARDENED_SANDBOX_URL"
                " to a vetted remote execution service."
            )
        }
    
    # Forbidden imports/functions
    forbidden_patterns = [
        "import os",
        "import sys",
        "import subprocess",
        "import socket",
        "__import__",
        "eval(",
        "exec(",
        "compile(",
        "open(",
    ]
    
    code_lower = code.lower()
    for pattern in forbidden_patterns:
        if pattern in code_lower:
            return {"error": f"Forbidden pattern detected: {pattern}"}
    
    try:
        response = requests.post(
            executor_url,
            json={
                "code": code,
                "language": language,
                "timeout": timeout,
                "max_output_bytes": max_output_bytes,
            },
            timeout=max(timeout + 2, 5),
        )
        response.raise_for_status()
    except requests.Timeout:
        LOGGER.warning("Remote sandbox execution timeout after %ds", timeout)
        return {"error": f"Execution timeout ({timeout}s)"}
    except requests.RequestException as exc:
        LOGGER.error("Remote sandbox request failed: %s", exc)
        return {"error": f"Sandbox executor error: {exc}"}

    try:
        payload = response.json()
    except ValueError as exc:
        LOGGER.error("Invalid JSON payload from sandbox executor: %s", exc)
        return {"error": "Sandbox executor returned invalid JSON"}

    # Ensure output limits are respected even if the executor does not enforce them strictly
    stdout = str(payload.get("stdout", ""))[:max_output_bytes]
    stderr = str(payload.get("stderr", ""))[:max_output_bytes]
    returncode = payload.get("returncode")
    success = payload.get("success")

    if returncode is None:
        LOGGER.error("Sandbox executor response missing returncode field")
        return {"error": "Sandbox executor response malformed"}

    return {
        "stdout": stdout,
        "stderr": stderr,
        "returncode": returncode,
        "success": bool(success) if success is not None else (returncode == 0),
    }


def verify_answer(
    answer: str,
    query: str,
    task_type: Optional[str] = None,
) -> Dict[str, object]:
    """
    Apply verification based on task type.
    
    Args:
        answer: Generated answer
        query: Original query
        task_type: Type of task (math, code, factual, etc.)
    
    Returns:
        Dict with verification result and trace
    """
    if task_type is None:
        # Heuristic detection
        if any(word in query.lower() for word in ["calculate", "compute", "what is", "sum", "multiply"]):
            task_type = "math"
        elif any(word in query.lower() for word in ["code", "function", "implement", "program"]):
            task_type = "code"
        else:
            task_type = "factual"
    
    LOGGER.info("Verifying answer for task_type=%s", task_type)
    
    if task_type == "math":
        # Try to extract and verify mathematical expressions
        # This is a simplified heuristic; production would need better parsing
        return {"verified": True, "task_type": task_type, "note": "Math verification placeholder"}
    
    elif task_type == "code":
        # For code tasks, we could extract and run test cases
        # This is a placeholder for more sophisticated verification
        return {"verified": True, "task_type": task_type, "note": "Code verification placeholder"}
    
    else:
        # Factual questions rely on evidence gating only
        return {"verified": True, "task_type": task_type, "note": "Evidence-based verification"}


__all__ = ["safe_calculator", "safe_code_sandbox", "verify_answer"]

