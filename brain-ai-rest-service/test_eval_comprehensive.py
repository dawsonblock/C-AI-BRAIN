"""
Comprehensive Evaluation Test Suite for Brain-AI RAG++
Tests factual recall, reasoning, coding, math, refusal, and edge cases
"""
from eval_harness import EvalHarness
import os

# Set base URL from environment or default
BASE_URL = os.getenv("BRAIN_AI_URL", "http://localhost:5001")

def main():
    harness = EvalHarness(base_url=BASE_URL, results_dir="eval_results")
    
    # ==================== Factual Recall Tests ====================
    
    harness.add_test_case(
        name="Rope Memory Storage",
        question="How did rope memory store bits?",
        expected_answer="through ferrite cores",
        category="factual",
        context="Rope memory stored bits by weaving wires through ferrite cores for 1 or around cores for 0.",
        use_multi_agent=False,
        min_confidence=0.7
    )
    
    harness.add_test_case(
        name="Apollo RAM Capacity",
        question="What was the RAM size of the Apollo Guidance Computer?",
        expected_answer="4KB",
        category="factual",
        context="The Apollo Guidance Computer (AGC) had only 4KB of RAM and 72KB of rope memory.",
        use_multi_agent=False,
        min_confidence=0.8
    )
    
    harness.add_test_case(
        name="Apollo ROM Type",
        question="What type of ROM did the Apollo computer use?",
        expected_answer="rope memory",
        category="factual",
        context="The Apollo Guidance Computer used rope memory as ROM, which was hand-woven by workers.",
        use_multi_agent=False,
        min_confidence=0.8
    )
    
    harness.add_test_case(
        name="Apollo Programming Language",
        question="What programming language was used for the Apollo Guidance Computer?",
        expected_answer="AGC Assembly",
        category="factual",
        context="The AGC was programmed in AGC Assembly language, later with the MAC macro assembler.",
        use_multi_agent=False,
        min_confidence=0.7
    )
    
    # ==================== Reasoning Tests ====================
    
    harness.add_test_case(
        name="Memory Capacity Comparison",
        question="Was the Apollo computer's RAM enough to run modern apps?",
        expected_answer="no",
        category="reasoning",
        context="The Apollo Guidance Computer had 4KB of RAM. Modern smartphone apps require tens of megabytes of RAM at minimum.",
        use_multi_agent=False,
        min_confidence=0.7
    )
    
    harness.add_test_case(
        name="Historical Context Inference",
        question="Why was rope memory necessary in the 1960s?",
        expected_answer="reliable and radiation-resistant",
        category="reasoning",
        context="In the 1960s, transistor memory was unreliable and expensive. Rope memory provided non-volatile, radiation-resistant storage needed for space missions.",
        use_multi_agent=True,
        min_confidence=0.7
    )
    
    # ==================== Multi-Agent Coding Tests ====================
    
    harness.add_test_case(
        name="Python Fibonacci Function",
        question="Write a Python function fib(n) that returns the nth Fibonacci number. Include unit tests.",
        expected_answer="def fib(n):",
        category="coding",
        context="",
        use_multi_agent=True,
        min_confidence=0.6
    )
    
    harness.add_test_case(
        name="Python Prime Checker",
        question="Write a Python function is_prime(n) with tests for n=2,3,4,17,100.",
        expected_answer="def is_prime(n):",
        category="coding",
        context="",
        use_multi_agent=True,
        min_confidence=0.6
    )
    
    harness.add_test_case(
        name="Python List Deduplication",
        question="Write a Python function deduplicate(lst) that removes duplicates while preserving order. Include tests.",
        expected_answer="def deduplicate(lst):",
        category="coding",
        context="",
        use_multi_agent=True,
        min_confidence=0.6
    )
    
    # ==================== Math Tests ====================
    
    harness.add_test_case(
        name="Basic Arithmetic",
        question="Calculate (15 * 3) + sqrt(81) - 2^3",
        expected_answer="46",
        category="math",
        context="",
        use_multi_agent=True,
        min_confidence=0.7
    )
    
    harness.add_test_case(
        name="Percentage Calculation",
        question="What is 37% of 842?",
        expected_answer="311.54",
        category="math",
        context="",
        use_multi_agent=True,
        min_confidence=0.7
    )
    
    harness.add_test_case(
        name="Multi-Step Math",
        question="If a train travels 60 mph for 2.5 hours, then 80 mph for 1.5 hours, how far does it travel?",
        expected_answer="270 miles",
        category="math",
        context="",
        use_multi_agent=True,
        min_confidence=0.7
    )
    
    # ==================== Refusal Tests (Low Evidence) ====================
    
    harness.add_test_case(
        name="Refusal - No Context",
        question="What color was Margaret Hamilton's car?",
        expected_answer="insufficient",
        category="refusal",
        context="",
        use_multi_agent=False,
        min_confidence=0.7
    )
    
    harness.add_test_case(
        name="Refusal - Contradictory Context",
        question="Was the AGC's RAM 2KB or 8KB?",
        expected_answer="4KB",
        category="refusal",
        context="The AGC had 4KB of RAM, not 2KB or 8KB.",
        use_multi_agent=False,
        min_confidence=0.7
    )
    
    # ==================== Edge Case Tests ====================
    
    harness.add_test_case(
        name="Edge - Empty Question",
        question="",
        expected_answer="error",
        category="edge_case",
        context="",
        use_multi_agent=False,
        min_confidence=0.0
    )
    
    harness.add_test_case(
        name="Edge - Very Long Question",
        question="What " + ("really " * 100) + "happened in the Apollo 11 mission?",
        expected_answer="Apollo 11",
        category="edge_case",
        context="Apollo 11 was the first mission to land humans on the Moon in 1969.",
        use_multi_agent=False,
        min_confidence=0.6
    )
    
    harness.add_test_case(
        name="Edge - Special Characters",
        question="What is the formula for E=mc^2?",
        expected_answer="energy equals mass times speed of light squared",
        category="edge_case",
        context="Einstein's famous equation E=mc² relates energy (E) to mass (m) and the speed of light (c).",
        use_multi_agent=False,
        min_confidence=0.7
    )
    
    # ==================== Complex Reasoning Tests ====================
    
    harness.add_test_case(
        name="Complex - Multi-Hop Reasoning",
        question="If rope memory took 6 months to manufacture and the AGC had 72KB, estimate the production time per KB.",
        expected_answer="2.5 days",
        category="complex",
        context="Rope memory for the AGC took approximately 6 months to manufacture. The AGC's ROM was 72KB of rope memory.",
        use_multi_agent=True,
        min_confidence=0.6
    )
    
    harness.add_test_case(
        name="Complex - Comparison",
        question="Compare the Apollo AGC's computing power to a modern smartphone.",
        expected_answer="millions of times more powerful",
        category="complex",
        context="The Apollo AGC had a 2.048 MHz clock and 4KB RAM. Modern smartphones have multi-GHz processors and gigabytes of RAM.",
        use_multi_agent=True,
        min_confidence=0.6
    )
    
    # ==================== Citation Quality Tests ====================
    
    harness.add_test_case(
        name="Citation - Specific Quote",
        question="Quote exactly what the AGC's word size was.",
        expected_answer="16-bit",
        category="citation",
        context="The Apollo Guidance Computer used a 16-bit word size with 1-bit parity, for a total of 17 bits per word.",
        use_multi_agent=False,
        min_confidence=0.8
    )
    
    harness.add_test_case(
        name="Citation - Multiple Sources",
        question="What were the main components of the AGC?",
        expected_answer="CPU, memory, I/O",
        category="citation",
        context="The AGC consisted of a CPU, 4KB RAM, 72KB ROM, and I/O interfaces for DSKY and sensors.",
        use_multi_agent=False,
        min_confidence=0.7
    )
    
    print(f"✅ Configured {len(harness.test_cases)} evaluation test cases")
    print("\n" + "="*60)
    print("Starting comprehensive evaluation run...")
    print("="*60 + "\n")
    
    # Run all tests
    harness.run_all_tests()
    
    print("\n" + "="*60)
    print("Evaluation complete! Check eval_results/ for detailed output.")
    print("="*60)


if __name__ == "__main__":
    main()

