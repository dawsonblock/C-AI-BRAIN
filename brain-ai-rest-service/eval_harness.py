"""
Evaluation harness for RAG++ system
Tests accuracy, groundedness, refusal rate, and latency
"""

import asyncio
import logging
import time
from typing import List, Dict, Any
from dataclasses import dataclass
import json
from pathlib import Path

logger = logging.getLogger(__name__)


@dataclass
class EvalQuestion:
    """Test question with expected answer"""
    question: str
    expected_answer: str
    context_docs: List[str]  # Document IDs that should be retrieved
    category: str  # e.g., "factual", "reasoning", "math", "code"
    min_confidence: float = 0.7


@dataclass
class EvalResult:
    """Result of a single evaluation"""
    question: str
    actual_answer: str
    expected_answer: str
    confidence: float
    refused: bool
    correct: bool
    citations_count: int
    processing_time_ms: int
    category: str
    error: str = None


class EvalHarness:
    """Evaluation harness for RAG++ system"""
    
    def __init__(self, test_cases_path: str = "data/eval/test_cases.json"):
        self.test_cases_path = Path(test_cases_path)
        self.test_cases: List[EvalQuestion] = []
        self.results: List[EvalResult] = []
    
    def load_test_cases(self):
        """Load test cases from JSON"""
        if not self.test_cases_path.exists():
            logger.warning(f"Test cases file not found: {self.test_cases_path}")
            self._create_default_test_cases()
            return
        
        try:
            with open(self.test_cases_path, 'r') as f:
                data = json.load(f)
            
            self.test_cases = [
                EvalQuestion(**case) for case in data["test_cases"]
            ]
            
            logger.info(f"✅ Loaded {len(self.test_cases)} test cases")
        except Exception as e:
            logger.error(f"Failed to load test cases: {e}")
            self._create_default_test_cases()
    
    def _create_default_test_cases(self):
        """Create default test cases"""
        self.test_cases = [
            EvalQuestion(
                question="What is 2 + 2?",
                expected_answer="4",
                context_docs=[],
                category="math",
                min_confidence=0.9
            ),
            EvalQuestion(
                question="What is the capital of France?",
                expected_answer="Paris",
                context_docs=[],
                category="factual",
                min_confidence=0.9
            ),
            EvalQuestion(
                question="Write a Python function to add two numbers",
                expected_answer="def add(a, b): return a + b",
                context_docs=[],
                category="code",
                min_confidence=0.7
            )
        ]
        
        # Save default test cases
        self.test_cases_path.parent.mkdir(parents=True, exist_ok=True)
        with open(self.test_cases_path, 'w') as f:
            json.dump({
                "test_cases": [
                    {
                        "question": tc.question,
                        "expected_answer": tc.expected_answer,
                        "context_docs": tc.context_docs,
                        "category": tc.category,
                        "min_confidence": tc.min_confidence
                    }
                    for tc in self.test_cases
                ]
            }, f, indent=2)
        
        logger.info(f"✅ Created {len(self.test_cases)} default test cases")
    
    async def run_evaluation(self, answer_endpoint_fn) -> Dict[str, Any]:
        """
        Run full evaluation suite
        
        Args:
            answer_endpoint_fn: Async function that takes question and returns answer dict
        
        Returns:
            Evaluation metrics and results
        """
        if not self.test_cases:
            self.load_test_cases()
        
        logger.info(f"🧪 Running evaluation on {len(self.test_cases)} test cases...")
        self.results = []
        
        for test_case in self.test_cases:
            try:
                start_time = time.time()
                
                # Call answer endpoint
                response = await answer_endpoint_fn(test_case.question)
                
                processing_time = int((time.time() - start_time) * 1000)
                
                # Evaluate answer
                correct = self._is_correct(
                    test_case.expected_answer,
                    response.get("answer", ""),
                    test_case.category
                )
                
                result = EvalResult(
                    question=test_case.question,
                    actual_answer=response.get("answer", ""),
                    expected_answer=test_case.expected_answer,
                    confidence=response.get("confidence", 0.0),
                    refused=response.get("refuse", False),
                    correct=correct,
                    citations_count=len(response.get("citations", [])),
                    processing_time_ms=processing_time,
                    category=test_case.category
                )
                
                self.results.append(result)
                logger.info(f"✓ {test_case.category}: {correct} (conf={result.confidence:.2f})")
                
            except Exception as e:
                logger.error(f"Test case failed: {test_case.question[:50]}... Error: {e}")
                self.results.append(EvalResult(
                    question=test_case.question,
                    actual_answer="",
                    expected_answer=test_case.expected_answer,
                    confidence=0.0,
                    refused=True,
                    correct=False,
                    citations_count=0,
                    processing_time_ms=0,
                    category=test_case.category,
                    error=str(e)
                ))
        
        # Calculate metrics
        metrics = self._calculate_metrics()
        
        # Save results
        self._save_results(metrics)
        
        return metrics
    
    def _is_correct(self, expected: str, actual: str, category: str) -> bool:
        """Check if answer is correct"""
        expected_lower = expected.lower().strip()
        actual_lower = actual.lower().strip()
        
        if category == "math":
            # Extract numbers
            import re
            expected_nums = re.findall(r'\d+\.?\d*', expected_lower)
            actual_nums = re.findall(r'\d+\.?\d*', actual_lower)
            return expected_nums == actual_nums
        
        elif category == "factual":
            # Check if expected answer is in actual answer
            return expected_lower in actual_lower
        
        elif category == "code":
            # Check for key elements
            key_elements = ["def", expected_lower.split()[1] if "def" in expected_lower else ""]
            return all(elem in actual_lower for elem in key_elements if elem)
        
        else:
            # General substring match
            return expected_lower in actual_lower
    
    def _calculate_metrics(self) -> Dict[str, Any]:
        """Calculate evaluation metrics"""
        if not self.results:
            return {}
        
        total = len(self.results)
        correct = sum(1 for r in self.results if r.correct)
        refused = sum(1 for r in self.results if r.refused)
        errors = sum(1 for r in self.results if r.error)
        
        avg_confidence = sum(r.confidence for r in self.results) / total
        avg_latency = sum(r.processing_time_ms for r in self.results) / total
        
        # Per-category metrics
        categories = {}
        for result in self.results:
            if result.category not in categories:
                categories[result.category] = {"total": 0, "correct": 0}
            categories[result.category]["total"] += 1
            if result.correct:
                categories[result.category]["correct"] += 1
        
        for cat in categories:
            categories[cat]["accuracy"] = categories[cat]["correct"] / categories[cat]["total"]
        
        metrics = {
            "total_cases": total,
            "correct": correct,
            "accuracy": correct / total,
            "refused_rate": refused / total,
            "error_rate": errors / total,
            "avg_confidence": avg_confidence,
            "avg_latency_ms": avg_latency,
            "by_category": categories
        }
        
        return metrics
    
    def _save_results(self, metrics: Dict):
        """Save evaluation results"""
        results_dir = Path("data/eval/results")
        results_dir.mkdir(parents=True, exist_ok=True)
        
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        results_file = results_dir / f"eval_{timestamp}.json"
        
        output = {
            "timestamp": timestamp,
            "metrics": metrics,
            "results": [
                {
                    "question": r.question,
                    "expected": r.expected_answer,
                    "actual": r.actual_answer,
                    "correct": r.correct,
                    "confidence": r.confidence,
                    "category": r.category,
                    "latency_ms": r.processing_time_ms
                }
                for r in self.results
            ]
        }
        
        with open(results_file, 'w') as f:
            json.dump(output, f, indent=2)
        
        logger.info(f"✅ Saved results to {results_file}")


# CLI for running evaluations
if __name__ == "__main__":
    import sys
    
    logging.basicConfig(level=logging.INFO)
    
    async def mock_answer_fn(question: str) -> Dict:
        """Mock answer function for testing"""
        return {
            "answer": "Mock answer",
            "confidence": 0.8,
            "refuse": False,
            "citations": []
        }
    
    harness = EvalHarness()
    metrics = asyncio.run(harness.run_evaluation(mock_answer_fn))
    
    print("\n=== Evaluation Results ===")
    print(f"Accuracy: {metrics['accuracy']:.2%}")
    print(f"Refusal Rate: {metrics['refused_rate']:.2%}")
    print(f"Avg Confidence: {metrics['avg_confidence']:.2f}")
    print(f"Avg Latency: {metrics['avg_latency_ms']:.0f}ms")
    print("\nBy Category:")
    for cat, stats in metrics['by_category'].items():
        print(f"  {cat}: {stats['accuracy']:.2%} ({stats['correct']}/{stats['total']})")

