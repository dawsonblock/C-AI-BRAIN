"""Security utilities including safe expression evaluation."""

import ast
import math
import operator as op
from typing import Any, Set, Dict, Callable
import logging

logger = logging.getLogger(__name__)

# Allowed AST node types for safe evaluation
ALLOWED_NODES: Set[type] = {
    ast.Expression,
    ast.BinOp,
    ast.UnaryOp,
    ast.Num,
    ast.Constant,
    ast.Add,
    ast.Sub,
    ast.Mult,
    ast.Div,
    ast.Pow,
    ast.Mod,
    ast.FloorDiv,
    ast.USub,
    ast.UAdd,
    ast.Load,
    ast.Call,
    ast.Name,
}

# Allowed mathematical functions
SAFE_FUNCTIONS: Dict[str, Callable] = {
    "sqrt": math.sqrt,
    "log": math.log,
    "log10": math.log10,
    "log2": math.log2,
    "exp": math.exp,
    "sin": math.sin,
    "cos": math.cos,
    "tan": math.tan,
    "asin": math.asin,
    "acos": math.acos,
    "atan": math.atan,
    "sinh": math.sinh,
    "cosh": math.cosh,
    "tanh": math.tanh,
    "ceil": math.ceil,
    "floor": math.floor,
    "abs": abs,
    "round": round,
    "min": min,
    "max": max,
    "sum": sum,
    "pi": math.pi,
    "e": math.e,
}

# Mathematical constants
SAFE_NAMES: Dict[str, Any] = {
    "pi": math.pi,
    "e": math.e,
    "tau": math.tau,
    "inf": math.inf,
    "nan": math.nan,
}


class SafeEvaluationError(ValueError):
    """Raised when expression evaluation fails safety checks."""
    pass


def validate_ast_node(node: ast.AST) -> None:
    """
    Recursively validate that all nodes in the AST are safe.
    
    Args:
        node: AST node to validate
        
    Raises:
        SafeEvaluationError: If unsafe node is detected
    """
    for n in ast.walk(node):
        node_type = type(n)
        
        # Check if node type is allowed
        if node_type not in ALLOWED_NODES:
            raise SafeEvaluationError(
                f"Disallowed node type: {node_type.__name__}"
            )
        
        # Additional validation for function calls
        if isinstance(n, ast.Call):
            if not isinstance(n.func, ast.Name):
                raise SafeEvaluationError(
                    "Only direct function calls are allowed"
                )
            
            func_name = n.func.id
            if func_name not in SAFE_FUNCTIONS:
                raise SafeEvaluationError(
                    f"Function not allowed: {func_name}"
                )
        
        # Validate name references
        if isinstance(n, ast.Name):
            name = n.id
            if name not in SAFE_NAMES and name not in SAFE_FUNCTIONS:
                # Allow names that will be resolved by safe functions
                if not isinstance(n.ctx, ast.Load):
                    raise SafeEvaluationError(
                        f"Name assignment not allowed: {name}"
                    )


def safe_eval(expression: str) -> float:
    """
    Safely evaluate a mathematical expression.
    
    Only allows basic arithmetic operations and whitelisted mathematical
    functions. No variable assignment, imports, or code execution.
    
    Args:
        expression: Mathematical expression as string
        
    Returns:
        Result of expression evaluation as float
        
    Raises:
        SafeEvaluationError: If expression contains unsafe operations
        ValueError: If expression is invalid Python syntax
        
    Examples:
        >>> safe_eval("2 + 2")
        4.0
        >>> safe_eval("sqrt(16) * 2")
        8.0
        >>> safe_eval("sin(pi / 2)")
        1.0
    """
    if not expression or not expression.strip():
        raise SafeEvaluationError("Empty expression")
    
    # Length check
    if len(expression) > 500:
        raise SafeEvaluationError("Expression too long")
    
    try:
        # Parse expression into AST
        node = ast.parse(expression.strip(), mode="eval")
    except SyntaxError as e:
        raise SafeEvaluationError(f"Invalid syntax: {e}")
    
    # Validate all nodes are safe
    try:
        validate_ast_node(node)
    except SafeEvaluationError as e:
        logger.warning(f"Unsafe expression blocked: {expression} - {e}")
        raise
    
    # Compile and evaluate with restricted namespace
    try:
        compiled = compile(node, "<expression>", "eval")
        namespace = {"__builtins__": {}}
        namespace.update(SAFE_FUNCTIONS)
        namespace.update(SAFE_NAMES)
        
        result = eval(compiled, namespace, {})
        
        # Ensure result is numeric
        if not isinstance(result, (int, float, complex)):
            raise SafeEvaluationError(
                f"Result must be numeric, got {type(result).__name__}"
            )
        
        # Convert to float
        if isinstance(result, complex):
            if result.imag != 0:
                raise SafeEvaluationError("Complex numbers not supported")
            result = result.real
        
        return float(result)
        
    except SafeEvaluationError:
        raise
    except ZeroDivisionError:
        raise SafeEvaluationError("Division by zero")
    except (ValueError, TypeError, OverflowError) as e:
        raise SafeEvaluationError(f"Evaluation error: {e}")
    except Exception as e:
        logger.error(f"Unexpected error in safe_eval: {e}", exc_info=True)
        raise SafeEvaluationError(f"Evaluation failed: {type(e).__name__}")
