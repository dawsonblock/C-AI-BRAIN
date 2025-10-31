"""
Multi-agent orchestration with Planner → Solvers×N → Verifier → Judge
Implements correction loop with early stop on verified pass
"""
import asyncio
from typing import List, Dict, Optional, Callable
from dataclasses import dataclass, field
from enum import Enum
import logging
import time

logger = logging.getLogger(__name__)


class SolutionStatus(Enum):
    """Status of a solution"""
    PENDING = "pending"
    VERIFIED = "verified"
    FAILED = "failed"
    REJECTED = "rejected"


@dataclass
class Solution:
    """Single solver's solution with verification results"""
    content: str
    solver_id: int
    confidence: float
    verification_result: Optional[Dict] = None
    score: float = 0.0
    status: SolutionStatus = SolutionStatus.PENDING
    reasoning: Optional[str] = None
    execution_time_ms: float = 0.0


@dataclass
class ExecutionPlan:
    """Plan created by planner agent"""
    strategy: str
    tools_needed: List[str]
    estimated_complexity: str  # "simple", "medium", "complex"
    suggested_n_solvers: int = 3


class MultiAgentOrchestrator:
    """
    Multi-agent system with Planner → Solvers → Verifier → Judge pattern
    
    Workflow:
    1. Planner analyzes question and creates strategy
    2. N solvers work in parallel following strategy
    3. Verifier checks each solution
    4. Early stop if any solution passes verification threshold
    5. Judge selects best if no early stop
    6. Optional retry round if all fail
    """
    
    def __init__(
        self,
        llm_client,
        n_solvers: int = 3,
        verification_threshold: float = 0.85,
        max_rounds: int = 1,
        enable_early_stop: bool = True,
        tools: Optional[Dict[str, Callable]] = None
    ):
        """
        Initialize multi-agent orchestrator
        
        Args:
            llm_client: DeepSeek client for LLM calls
            n_solvers: Number of parallel solvers
            verification_threshold: Score threshold for acceptance
            max_rounds: Maximum retry rounds
            enable_early_stop: Stop on first verified solution
            tools: Available tools (calculator, code_exec, etc.)
        """
        self.llm = llm_client
        self.n_solvers = n_solvers
        self.verification_threshold = verification_threshold
        self.max_rounds = max_rounds
        self.enable_early_stop = enable_early_stop
        self.tools = tools or {}
        
        logger.info(
            f"🤖 MultiAgent initialized: n_solvers={n_solvers}, "
            f"threshold={verification_threshold}, max_rounds={max_rounds}"
        )
    
    async def solve(
        self,
        question: str,
        context: str,
        force_multi_agent: bool = False
    ) -> Dict:
        """
        Solve question using multi-agent orchestration
        
        Args:
            question: User question
            context: Retrieved context
            force_multi_agent: Force multi-agent even for simple questions
        
        Returns:
            {
                "answer": str,
                "solver_id": int,
                "confidence": float,
                "verification": {...},
                "score": float,
                "method": str,
                "plan": {...},
                "all_solutions": [...],
                "execution_time_ms": float
            }
        """
        start_time = time.time()
        
        try:
            # Step 1: Plan
            plan = await self._plan(question, context)
            logger.info(
                f"📋 Plan created: strategy='{plan.strategy[:50]}...', "
                f"complexity={plan.estimated_complexity}"
            )
            
            # Check if multi-agent is needed
            if not force_multi_agent and plan.estimated_complexity == "simple":
                logger.info("⚡ Simple question, using single solver")
                return await self._simple_solve(question, context, plan)
            
            # Step 2-5: Multi-agent loop
            best_solution = None
            all_solutions = []
            
            for round_idx in range(self.max_rounds):
                logger.info(f"🔄 Round {round_idx + 1}/{self.max_rounds}")
                
                # Step 2: Generate solutions in parallel
                solutions = await self._solve_parallel(
                    question, context, plan, round_idx
                )
                all_solutions.extend(solutions)
                
                # Step 3: Verify each solution
                for solution in solutions:
                    await self._verify(solution, question, plan)
                    
                    # Step 4: Early stop check
                    if (self.enable_early_stop and 
                        solution.status == SolutionStatus.VERIFIED and
                        solution.score >= self.verification_threshold):
                        
                        logger.info(
                            f"✅ Early stop: Solver {solution.solver_id} "
                            f"verified (score={solution.score:.2f})"
                        )
                        
                        return self._format_result(
                            solution, "early_stop", plan, all_solutions,
                            time.time() - start_time
                        )
                
                # Step 5: Judge best solution
                best_solution = self._judge(solutions)
                
                # If best solution meets threshold, return it
                if best_solution.score >= self.verification_threshold:
                    break
                
                # Log round results
                logger.info(
                    f"Round {round_idx + 1} best score: {best_solution.score:.2f} "
                    f"(threshold={self.verification_threshold})"
                )
            
            # Return best solution after all rounds
            logger.info(
                f"🏆 Best solution: Solver {best_solution.solver_id} "
                f"(score={best_solution.score:.2f})"
            )
            
            return self._format_result(
                best_solution, "judged", plan, all_solutions,
                time.time() - start_time
            )
        
        except Exception as e:
            logger.error(f"❌ Multi-agent solve failed: {e}")
            raise
    
    async def _plan(
        self, 
        question: str, 
        context: str
    ) -> ExecutionPlan:
        """
        Planner agent creates solution strategy
        
        Analyzes question complexity and suggests approach
        """
        messages = [
            {
                "role": "system",
                "content": """You are a planning agent. Analyze the question and create a strategy.
                
Consider:
- Question complexity (simple/medium/complex)
- Required reasoning steps
- Tools needed (calculator, code execution, etc.)
- Number of solvers recommended

Be concise."""
            },
            {
                "role": "user",
                "content": f"""Question: {question}

Context: {context[:500]}...

Available tools: {list(self.tools.keys())}

Create strategy:"""
            }
        ]
        
        result = self.llm.generate(messages, config=None)
        strategy = result["content"]
        
        # Parse complexity (simple heuristic)
        complexity = "medium"
        if any(w in question.lower() for w in ["simple", "what is", "define"]):
            complexity = "simple"
        elif any(w in question.lower() for w in ["prove", "derive", "optimize"]):
            complexity = "complex"
        
        # Suggest solver count based on complexity
        n_solvers = {
            "simple": 1,
            "medium": 3,
            "complex": 5
        }.get(complexity, 3)
        
        return ExecutionPlan(
            strategy=strategy,
            tools_needed=list(self.tools.keys()),
            estimated_complexity=complexity,
            suggested_n_solvers=min(n_solvers, self.n_solvers)
        )
    
    async def _solve_parallel(
        self,
        question: str,
        context: str,
        plan: ExecutionPlan,
        round_idx: int
    ) -> List[Solution]:
        """Generate N solutions in parallel"""
        n = plan.suggested_n_solvers
        
        tasks = [
            self._solve_single(question, context, plan, solver_id, round_idx)
            for solver_id in range(n)
        ]
        
        solutions = await asyncio.gather(*tasks, return_exceptions=True)
        
        # Filter out exceptions
        valid_solutions = []
        for i, sol in enumerate(solutions):
            if isinstance(sol, Exception):
                logger.error(f"Solver {i} failed: {sol}")
            else:
                valid_solutions.append(sol)
        
        return valid_solutions
    
    async def _solve_single(
        self,
        question: str,
        context: str,
        plan: ExecutionPlan,
        solver_id: int,
        round_idx: int
    ) -> Solution:
        """Single solver generates answer following plan"""
        start_time = time.time()
        
        # Vary temperature by solver ID
        # Solver 0: greedy (temp=0)
        # Others: more exploratory
        temperature = 0.0 if solver_id == 0 else min(0.3 + solver_id * 0.1, 0.7)
        
        messages = [
            {
                "role": "system",
                "content": f"""You are Solver {solver_id}. Follow the strategy precisely.

Strategy: {plan.strategy}

Be accurate and cite evidence from context."""
            },
            {
                "role": "user",
                "content": f"Context:\n{context}\n\nQuestion: {question}\n\nAnswer:"
            }
        ]
        
        from llm_deepseek import GenerationConfig
        config = GenerationConfig(temperature=temperature, max_tokens=1024)
        
        result = self.llm.generate(messages, config=config)
        
        execution_time = (time.time() - start_time) * 1000
        
        return Solution(
            content=result["content"],
            solver_id=solver_id,
            confidence=0.7,  # Default, updated by verifier
            execution_time_ms=execution_time
        )
    
    async def _simple_solve(
        self,
        question: str,
        context: str,
        plan: ExecutionPlan
    ) -> Dict:
        """Fast path for simple questions - single solver"""
        start_time = time.time()
        
        solution = await self._solve_single(question, context, plan, 0, 0)
        await self._verify(solution, question, plan)
        
        return self._format_result(
            solution, "simple", plan, [solution],
            (time.time() - start_time) * 1000
        )
    
    async def _verify(
        self,
        solution: Solution,
        question: str,
        plan: ExecutionPlan
    ) -> None:
        """
        Verifier agent checks solution correctness
        
        Uses critic model to score and identify issues
        Updates solution in-place
        """
        # Run tool verification if available
        tool_results = []
        if "code_exec" in self.tools and "```" in solution.content:
            try:
                tool_results.append(
                    await self._execute_code_blocks(solution.content)
                )
            except Exception as e:
                logger.warning(f"Code execution failed: {e}")
        
        # LLM verification
        messages = [
            {
                "role": "system",
                "content": """You are a critic. Verify the answer is:
1. Correct and accurate
2. Well-supported by evidence
3. Complete

Score 0-1. Identify specific issues."""
            },
            {
                "role": "user",
                "content": f"""Question: {question}

Answer: {solution.content}

Tool results: {tool_results}

Verify (JSON format):"""
            }
        ]
        
        schema = {
            "type": "json_schema",
            "json_schema": {
                "name": "verification",
                "strict": True,
                "schema": {
                    "type": "object",
                    "properties": {
                        "passed": {"type": "boolean"},
                        "score": {"type": "number", "minimum": 0, "maximum": 1},
                        "issues": {"type": "array", "items": {"type": "string"}},
                        "confidence": {"type": "number", "minimum": 0, "maximum": 1}
                    },
                    "required": ["passed", "score", "issues", "confidence"],
                    "additionalProperties": False
                }
            }
        }
        
        result = self.llm.generate(messages, response_format=schema)
        verification = result["content"]
        
        # Update solution
        solution.verification_result = verification
        solution.score = verification["score"]
        solution.confidence = verification["confidence"]
        solution.status = (
            SolutionStatus.VERIFIED if verification["passed"] 
            else SolutionStatus.FAILED
        )
    
    def _judge(self, solutions: List[Solution]) -> Solution:
        """Adjudicator selects best solution based on scores"""
        if not solutions:
            raise ValueError("No solutions to judge")
        
        # Sort by score descending, then by confidence
        sorted_solutions = sorted(
            solutions,
            key=lambda s: (s.score, s.confidence),
            reverse=True
        )
        
        best = sorted_solutions[0]
        logger.info(
            f"Judge selected: Solver {best.solver_id} "
            f"(score={best.score:.2f}, conf={best.confidence:.2f})"
        )
        
        return best
    
    def _format_result(
        self,
        solution: Solution,
        method: str,
        plan: ExecutionPlan,
        all_solutions: List[Solution],
        execution_time_ms: float
    ) -> Dict:
        """Format final result"""
        return {
            "answer": solution.content,
            "solver_id": solution.solver_id,
            "confidence": solution.confidence,
            "verification": solution.verification_result,
            "score": solution.score,
            "status": solution.status.value,
            "method": method,
            "plan": {
                "strategy": plan.strategy,
                "complexity": plan.estimated_complexity,
                "n_solvers": plan.suggested_n_solvers
            },
            "all_solutions": [
                {
                    "solver_id": s.solver_id,
                    "score": s.score,
                    "confidence": s.confidence,
                    "status": s.status.value
                }
                for s in all_solutions
            ],
            "execution_time_ms": execution_time_ms,
            "total_solutions_generated": len(all_solutions)
        }
    
    async def _execute_code_blocks(self, content: str) -> Dict:
        """Execute code blocks in solution (simplified)"""
        # TODO: Implement safe code execution sandbox
        # For now, just check syntax
        import re
        code_blocks = re.findall(r'```python\n(.*?)\n```', content, re.DOTALL)
        
        results = []
        for code in code_blocks:
            try:
                # Syntax check only
                compile(code, '<string>', 'exec')
                results.append({"status": "syntax_ok", "code": code[:100]})
            except SyntaxError as e:
                results.append({"status": "syntax_error", "error": str(e)})
        
        return {"code_blocks": len(code_blocks), "results": results}

