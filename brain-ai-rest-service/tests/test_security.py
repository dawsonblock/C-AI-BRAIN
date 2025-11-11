"""Tests for security utilities, especially safe_eval."""

import pytest
from app.security import safe_eval, SafeEvaluationError

# Test cases for safe and valid expressions
VALID_EXPRESSIONS = [
    ("2 + 2", 4.0),
    ("10 - 5.5", 4.5),
    ("2 * 3", 6.0),
    ("10 / 2", 5.0),
    ("2 ** 3", 8.0),
    ("10 % 3", 1.0),
    ("10 // 3", 3.0),
    ("-5", -5.0),
    ("+5", 5.0),
    ("sqrt(16)", 4.0),
    ("log(10)", 2.302585092994046),
    ("sin(pi / 2)", 1.0),
    ("cos(pi)", -1.0),
    ("e", 2.718281828459045),
    ("sum([1, 2, 3])", 6.0),
    ("max(1, 10, 2)", 10.0),
    ("min(-1, -10)", -10.0),
    ("round(3.14159, 2)", 3.14),
]

# Test cases for unsafe or invalid expressions
INVALID_EXPRESSIONS = [
    # Arbitrary code execution attempts
    "__import__('os').system('echo pwned')",
    "eval('1+1')",
    "[c for c in ().__class__.__base__.__subclasses__() if c.__name__ == 'BuiltinImporter'][0]().load_module('os').system('echo pwned')",
    
    # Attribute access
    "''.__class__",
    "(lambda: 1).__globals__",
    
    # Name assignment
    "x = 5",
    
    # Unsafe function calls
    "globals()",
    "locals()",
    "vars()",
    
    # Syntax errors
    "2 +",
    "((",
    
    # Other invalid patterns
    "import math",
    "def f(): pass",
    "lambda x: x + 1",
    "a if b else c",
]


@pytest.mark.parametrize("expression, expected", VALID_EXPRESSIONS)
def test_safe_eval_valid(expression, expected):
    """Test that safe_eval correctly evaluates valid mathematical expressions."""
    assert safe_eval(expression) == pytest.approx(expected)


@pytest.mark.parametrize("expression", INVALID_EXPRESSIONS)
def test_safe_eval_invalid(expression):
    """Test that safe_eval raises SafeEvaluationError for unsafe or invalid expressions."""
    with pytest.raises(SafeEvaluationError):
        safe_eval(expression)


def test_safe_eval_long_expression():
    """Test that long expressions are rejected."""
    long_expr = "1" + "+1" * 500
    with pytest.raises(SafeEvaluationError, match="Expression too long"):
        safe_eval(long_expr)


def test_safe_eval_empty_expression():
    """Test that empty expressions are rejected."""
    with pytest.raises(SafeEvaluationError, match="Empty expression"):
        safe_eval("")
    with pytest.raises(SafeEvaluationError, match="Empty expression"):
        safe_eval("   ")

def test_safe_eval_division_by_zero():
    """Test that division by zero is handled."""
    with pytest.raises(SafeEvaluationError, match="Division by zero"):
        safe_eval("1 / 0")

def test_safe_eval_complex_numbers():
    """Test that complex numbers are not supported."""
    with pytest.raises(SafeEvaluationError, match="Evaluation error: math domain error"):
        safe_eval("sqrt(-1)")
