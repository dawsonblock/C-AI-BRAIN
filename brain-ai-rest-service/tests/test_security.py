"""Tests for security module - safe evaluation."""

import pytest
from app.security import safe_eval, SafeEvaluationError


class TestSafeEval:
    """Test safe mathematical expression evaluation."""
    
    def test_basic_arithmetic(self):
        """Test basic arithmetic operations."""
        assert safe_eval("2 + 2") == 4.0
        assert safe_eval("10 - 3") == 7.0
        assert safe_eval("6 * 7") == 42.0
        assert safe_eval("15 / 3") == 5.0
        assert safe_eval("10 // 3") == 3.0
        assert safe_eval("10 % 3") == 1.0
        assert safe_eval("2 ** 8") == 256.0
    
    def test_mathematical_functions(self):
        """Test whitelisted mathematical functions."""
        import math
        assert abs(safe_eval("sqrt(16)") - 4.0) < 0.001
        assert abs(safe_eval("sin(0)") - 0.0) < 0.001
        assert abs(safe_eval("cos(0)") - 1.0) < 0.001
        assert abs(safe_eval("log(2.718281828)") - 1.0) < 0.01
        assert abs(safe_eval("exp(1)") - math.e) < 0.001
    
    def test_constants(self):
        """Test mathematical constants."""
        import math
        assert abs(safe_eval("pi") - math.pi) < 0.001
        assert abs(safe_eval("e") - math.e) < 0.001
        assert abs(safe_eval("pi * 2") - math.pi * 2) < 0.001
    
    def test_complex_expressions(self):
        """Test complex valid expressions."""
        assert safe_eval("(2 + 3) * 4") == 20.0
        assert safe_eval("sqrt(16) + sqrt(9)") == 7.0
        assert abs(safe_eval("sin(pi / 2)") - 1.0) < 0.001
        assert safe_eval("max(10, 20, 5)") == 20.0
        assert safe_eval("min(10, 20, 5)") == 5.0
        assert safe_eval("abs(-42)") == 42.0
    
    def test_reject_imports(self):
        """Test that imports are rejected."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("import os")
        with pytest.raises(SafeEvaluationError):
            safe_eval("__import__('os')")
    
    def test_reject_exec_eval(self):
        """Test that exec/eval are rejected."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("eval('2 + 2')")
        with pytest.raises(SafeEvaluationError):
            safe_eval("exec('print(1)')")
    
    def test_reject_file_operations(self):
        """Test that file operations are rejected."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("open('/etc/passwd')")
    
    def test_reject_unauthorized_functions(self):
        """Test that unauthorized functions are rejected."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("print('hello')")
        with pytest.raises(SafeEvaluationError):
            safe_eval("input()")
    
    def test_reject_variable_assignment(self):
        """Test that variable assignment is rejected."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("x = 42")
    
    def test_reject_dunder_access(self):
        """Test that dunder method access is rejected."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("().__class__.__bases__")
    
    def test_empty_expression(self):
        """Test that empty expressions are rejected."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("")
        with pytest.raises(SafeEvaluationError):
            safe_eval("   ")
    
    def test_length_limit(self):
        """Test expression length limit."""
        long_expr = "1 + " * 200 + "1"
        with pytest.raises(SafeEvaluationError):
            safe_eval(long_expr)
    
    def test_syntax_errors(self):
        """Test invalid syntax is caught."""
        with pytest.raises(SafeEvaluationError):
            safe_eval("2 +")
        with pytest.raises(SafeEvaluationError):
            safe_eval(")(")
    
    def test_division_by_zero(self):
        """Test division by zero handling."""
        with pytest.raises(SafeEvaluationError, match="zero"):
            safe_eval("1 / 0")
    
    def test_whitespace_handling(self):
        """Test expressions with various whitespace."""
        assert safe_eval("  2 + 2  ") == 4.0
        assert safe_eval("2+2") == 4.0
        assert safe_eval("2  +  2") == 4.0
