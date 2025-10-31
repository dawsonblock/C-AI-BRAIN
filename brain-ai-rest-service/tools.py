"""
Verifier tools for multi-agent orchestration
Provides calculator, code sandbox, and SQL readers
"""

import ast
import logging
import re
import math
import operator
from typing import Dict, List, Any, Optional
import subprocess
import tempfile
import os

logger = logging.getLogger(__name__)


class Calculator:
    """Safe mathematical expression evaluator"""
    
    # Allowed operators and functions
    ALLOWED_OPERATORS = {
        ast.Add: operator.add,
        ast.Sub: operator.sub,
        ast.Mult: operator.mul,
        ast.Div: operator.truediv,
        ast.Pow: operator.pow,
        ast.USub: operator.neg,
        ast.UAdd: operator.pos,
        ast.Mod: operator.mod,
        ast.FloorDiv: operator.floordiv,
    }
    
    ALLOWED_FUNCTIONS = {
        'abs': abs,
        'round': round,
        'min': min,
        'max': max,
        'sum': sum,
        'pow': pow,
        'sqrt': math.sqrt,
        'sin': math.sin,
        'cos': math.cos,
        'tan': math.tan,
        'log': math.log,
        'log10': math.log10,
        'exp': math.exp,
        'ceil': math.ceil,
        'floor': math.floor,
        'pi': math.pi,
        'e': math.e,
    }
    
    def evaluate(self, expression: str) -> Dict[str, Any]:
        """
        Safely evaluate mathematical expression
        
        Returns:
            {"result": float/int, "success": bool, "error": str}
        """
        try:
            # Parse the expression
            tree = ast.parse(expression, mode='eval')
            
            # Evaluate safely
            result = self._eval_node(tree.body)
            
            return {
                "result": result,
                "success": True,
                "expression": expression
            }
            
        except Exception as e:
            logger.error(f"Calculator error: {e}")
            return {
                "result": None,
                "success": False,
                "error": str(e),
                "expression": expression
            }
    
    def _eval_node(self, node):
        """Recursively evaluate AST node"""
        if isinstance(node, ast.Constant):
            return node.value
        
        elif isinstance(node, ast.Num):  # Python < 3.8
            return node.n
        
        elif isinstance(node, ast.BinOp):
            left = self._eval_node(node.left)
            right = self._eval_node(node.right)
            op_type = type(node.op)
            
            if op_type not in self.ALLOWED_OPERATORS:
                raise ValueError(f"Operator {op_type.__name__} not allowed")
            
            return self.ALLOWED_OPERATORS[op_type](left, right)
        
        elif isinstance(node, ast.UnaryOp):
            operand = self._eval_node(node.operand)
            op_type = type(node.op)
            
            if op_type not in self.ALLOWED_OPERATORS:
                raise ValueError(f"Operator {op_type.__name__} not allowed")
            
            return self.ALLOWED_OPERATORS[op_type](operand)
        
        elif isinstance(node, ast.Call):
            func_name = node.func.id if isinstance(node.func, ast.Name) else None
            
            if func_name not in self.ALLOWED_FUNCTIONS:
                raise ValueError(f"Function {func_name} not allowed")
            
            args = [self._eval_node(arg) for arg in node.args]
            return self.ALLOWED_FUNCTIONS[func_name](*args)
        
        elif isinstance(node, ast.Name):
            # Allow only predefined constants
            if node.id in self.ALLOWED_FUNCTIONS:
                return self.ALLOWED_FUNCTIONS[node.id]
            raise ValueError(f"Name {node.id} not allowed")
        
        else:
            raise ValueError(f"Node type {type(node).__name__} not allowed")


class CodeSandbox:
    """Safe Python code execution sandbox"""
    
    def __init__(
        self,
        timeout_seconds: int = 5,
        max_output_lines: int = 100,
        allowed_imports: List[str] = None
    ):
        self.timeout = timeout_seconds
        self.max_output_lines = max_output_lines
        self.allowed_imports = allowed_imports or ['math', 'statistics', 'itertools', 'functools']
    
    def execute(self, code: str, test_code: Optional[str] = None) -> Dict[str, Any]:
        """
        Execute Python code in isolated process
        
        Args:
            code: Python code to execute
            test_code: Optional test code to run
        
        Returns:
            {"success": bool, "output": str, "error": str, "tests_passed": bool}
        """
        try:
            # Validate code safety
            validation = self._validate_code(code)
            if not validation["safe"]:
                return {
                    "success": False,
                    "output": "",
                    "error": f"Code validation failed: {validation['reason']}",
                    "tests_passed": False
                }
            
            # Create temp file
            with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False) as f:
                f.write(code)
                if test_code:
                    f.write("\n\n# Tests\n")
                    f.write(test_code)
                code_file = f.name
            
            try:
                # Execute in subprocess with timeout
                result = subprocess.run(
                    ['python3', code_file],
                    capture_output=True,
                    text=True,
                    timeout=self.timeout
                )
                
                output = result.stdout
                error = result.stderr
                success = result.returncode == 0
                
                # Limit output
                output_lines = output.split('\n')
                if len(output_lines) > self.max_output_lines:
                    output = '\n'.join(output_lines[:self.max_output_lines]) + f"\n... ({len(output_lines) - self.max_output_lines} more lines)"
                
                return {
                    "success": success,
                    "output": output,
                    "error": error if error else None,
                    "tests_passed": success and "FAILED" not in output and "ERROR" not in output,
                    "return_code": result.returncode
                }
                
            finally:
                # Cleanup
                os.unlink(code_file)
                
        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "output": "",
                "error": f"Execution timed out after {self.timeout} seconds",
                "tests_passed": False
            }
        except Exception as e:
            logger.error(f"Code execution error: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "tests_passed": False
            }
    
    def _validate_code(self, code: str) -> Dict[str, Any]:
        """Validate code for safety"""
        # Forbidden patterns
        forbidden = [
            r'\bos\.',
            r'\bsubprocess\.',
            r'\bsys\.',
            r'\bopen\(',
            r'\bexec\(',
            r'\beval\(',
            r'\b__import__\(',
            r'\bcompile\(',
            r'\bglobals\(',
            r'\blocals\(',
            r'\bsetattr\(',
            r'\bdelattr\(',
            r'\bimport\s+(?!' + '|'.join(self.allowed_imports) + r')',
        ]
        
        for pattern in forbidden:
            if re.search(pattern, code):
                return {
                    "safe": False,
                    "reason": f"Forbidden pattern detected: {pattern}"
                }
        
        # Try parsing
        try:
            ast.parse(code)
        except SyntaxError as e:
            return {
                "safe": False,
                "reason": f"Syntax error: {e}"
            }
        
        return {"safe": True}


class SQLReader:
    """Simple in-memory SQL executor for CSV/table data"""
    
    def __init__(self):
        try:
            import sqlite3
            self.sqlite3 = sqlite3
        except ImportError:
            self.sqlite3 = None
            logger.warning("sqlite3 not available - SQL tools disabled")
    
    def query_csv(self, csv_data: str, query: str, table_name: str = "data") -> Dict[str, Any]:
        """
        Execute SQL query on CSV data
        
        Args:
            csv_data: CSV string
            query: SQL query
            table_name: Table name to use
        
        Returns:
            {"success": bool, "results": List[Dict], "columns": List[str], "error": str}
        """
        if not self.sqlite3:
            return {
                "success": False,
                "error": "sqlite3 not available"
            }
        
        try:
            import csv
            import io
            
            # Parse CSV
            reader = csv.DictReader(io.StringIO(csv_data))
            rows = list(reader)
            
            if not rows:
                return {
                    "success": False,
                    "error": "No data in CSV"
                }
            
            # Create in-memory database
            conn = self.sqlite3.connect(':memory:')
            cursor = conn.cursor()
            
            # Create table
            columns = list(rows[0].keys())
            create_sql = f"CREATE TABLE {table_name} ({', '.join([f'{col} TEXT' for col in columns])})"
            cursor.execute(create_sql)
            
            # Insert data
            for row in rows:
                values = [row[col] for col in columns]
                placeholders = ', '.join(['?' for _ in columns])
                cursor.execute(f"INSERT INTO {table_name} VALUES ({placeholders})", values)
            
            # Execute query
            cursor.execute(query)
            results = cursor.fetchall()
            result_columns = [desc[0] for desc in cursor.description]
            
            # Format results
            formatted_results = [
                dict(zip(result_columns, row))
                for row in results
            ]
            
            conn.close()
            
            return {
                "success": True,
                "results": formatted_results,
                "columns": result_columns,
                "row_count": len(formatted_results)
            }
            
        except Exception as e:
            logger.error(f"SQL execution error: {e}")
            return {
                "success": False,
                "error": str(e)
            }


# Tool registry for multi-agent verifier
TOOL_REGISTRY = {
    "calculator": Calculator(),
    "code_sandbox": CodeSandbox(timeout_seconds=5),
    "sql_reader": SQLReader()
}


def get_tool(tool_name: str):
    """Get tool instance by name"""
    return TOOL_REGISTRY.get(tool_name)

