"""Canonical facts store for high-confidence Q/A triples with citations"""
import sqlite3
import json
import hashlib
from typing import List, Dict, Optional
from datetime import datetime
from pathlib import Path
import logging

logger = logging.getLogger(__name__)


class FactsStore:
    """
    Fast KV store for verified Q→A→citations triples
    
    Features:
    - Only stores high-confidence facts (>0.85)
    - Fast lookup by question hash
    - Access statistics tracking
    - Automatic cleanup of low-value facts
    """
    
    def __init__(self, db_path: str = "data/facts.db", confidence_threshold: float = 0.85):
        """
        Initialize facts store
        
        Args:
            db_path: Path to SQLite database
            confidence_threshold: Minimum confidence to store facts
        """
        self.db_path = db_path
        self.confidence_threshold = confidence_threshold
        
        # Ensure data directory exists
        Path(db_path).parent.mkdir(parents=True, exist_ok=True)
        
        self._init_db()
        logger.info(f"✅ FactsStore initialized at {db_path} (threshold={confidence_threshold})")
    
    def _init_db(self):
        """Initialize SQLite database schema"""
        conn = sqlite3.connect(self.db_path)
        
        # Main facts table
        conn.execute("""
            CREATE TABLE IF NOT EXISTS facts (
                question_hash TEXT PRIMARY KEY,
                question TEXT NOT NULL,
                answer TEXT NOT NULL,
                citations TEXT NOT NULL,
                confidence REAL NOT NULL,
                verified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                access_count INTEGER DEFAULT 0,
                last_accessed TIMESTAMP,
                source TEXT,
                model TEXT
            )
        """)
        
        # Indexes for fast lookup
        conn.execute("CREATE INDEX IF NOT EXISTS idx_confidence ON facts(confidence)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_access_count ON facts(access_count)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_verified_at ON facts(verified_at)")
        
        conn.commit()
        conn.close()
    
    def add_fact(
        self,
        question: str,
        answer: str,
        citations: List[str],
        confidence: float,
        source: str = "unknown",
        model: str = "deepseek-chat"
    ) -> bool:
        """
        Add verified fact to store (only if meets confidence threshold)
        
        Args:
            question: Question text
            answer: Verified answer
            citations: List of citation texts/IDs
            confidence: Confidence score (0-1)
            source: Source system (e.g., "multi_agent", "single")
            model: Model used to generate answer
        
        Returns:
            True if fact was stored, False if below threshold
        """
        # Filter by confidence threshold
        if confidence < self.confidence_threshold:
            logger.debug(f"Fact rejected: confidence {confidence:.2f} < {self.confidence_threshold}")
            return False
        
        # Hash question for fast lookup
        q_hash = self._hash_question(question)
        
        conn = sqlite3.connect(self.db_path)
        try:
            conn.execute("""
                INSERT OR REPLACE INTO facts 
                (question_hash, question, answer, citations, confidence, 
                 verified_at, access_count, source, model)
                VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP, 
                        COALESCE((SELECT access_count FROM facts WHERE question_hash = ?), 0),
                        ?, ?)
            """, (
                q_hash, question, answer, json.dumps(citations), 
                confidence, q_hash, source, model
            ))
            conn.commit()
            
            logger.info(f"✅ Stored fact (confidence={confidence:.2f}, source={source})")
            return True
            
        except Exception as e:
            logger.error(f"Failed to store fact: {e}")
            return False
        finally:
            conn.close()
    
    def lookup(
        self, 
        question: str, 
        fuzzy_match: bool = False
    ) -> Optional[Dict]:
        """
        Lookup fact by question
        
        Args:
            question: Question to look up
            fuzzy_match: Enable fuzzy matching (slower, not yet implemented)
        
        Returns:
            Dict with question, answer, citations, confidence, etc. or None
        """
        q_hash = self._hash_question(question)
        
        conn = sqlite3.connect(self.db_path)
        cursor = conn.execute("""
            SELECT question, answer, citations, confidence, 
                   access_count, verified_at, source, model
            FROM facts
            WHERE question_hash = ?
        """, (q_hash,))
        
        row = cursor.fetchone()
        conn.close()
        
        if row:
            # Update access statistics
            self._update_access(q_hash)
            
            return {
                "question": row[0],
                "answer": row[1],
                "citations": json.loads(row[2]),
                "confidence": row[3],
                "access_count": row[4],
                "verified_at": row[5],
                "source": row[6],
                "model": row[7],
                "cached": True
            }
        
        # TODO: Implement fuzzy matching using embeddings
        if fuzzy_match:
            logger.warning("Fuzzy matching not yet implemented")
        
        return None
    
    def _update_access(self, question_hash: str):
        """Update access statistics for a fact"""
        conn = sqlite3.connect(self.db_path)
        try:
            conn.execute("""
                UPDATE facts
                SET access_count = access_count + 1,
                    last_accessed = CURRENT_TIMESTAMP
                WHERE question_hash = ?
            """, (question_hash,))
            conn.commit()
        except Exception as e:
            logger.error(f"Failed to update access stats: {e}")
        finally:
            conn.close()
    
    def _hash_question(self, question: str) -> str:
        """Generate hash for question (case-insensitive, whitespace-normalized)"""
        # Normalize: lowercase, strip, collapse whitespace
        normalized = " ".join(question.lower().strip().split())
        return hashlib.sha256(normalized.encode()).hexdigest()
    
    def get_stats(self) -> Dict:
        """Get store statistics"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.execute("""
            SELECT 
                COUNT(*) as total_facts,
                AVG(confidence) as avg_confidence,
                MIN(confidence) as min_confidence,
                MAX(confidence) as max_confidence,
                SUM(access_count) as total_accesses,
                MAX(access_count) as max_accesses
            FROM facts
        """)
        row = cursor.fetchone()
        
        # Get top accessed facts
        cursor2 = conn.execute("""
            SELECT question, access_count
            FROM facts
            ORDER BY access_count DESC
            LIMIT 5
        """)
        top_facts = cursor2.fetchall()
        
        conn.close()
        
        return {
            "total_facts": row[0] or 0,
            "avg_confidence": row[1] or 0,
            "min_confidence": row[2] or 0,
            "max_confidence": row[3] or 0,
            "total_accesses": row[4] or 0,
            "max_accesses": row[5] or 0,
            "top_accessed": [
                {"question": q, "count": c} for q, c in top_facts
            ]
        }
    
    def cleanup_low_value_facts(
        self, 
        min_access_count: int = 1,
        max_age_days: int = 90
    ) -> int:
        """
        Remove low-value facts (rarely accessed, old)
        
        Args:
            min_access_count: Keep facts with at least this many accesses
            max_age_days: Remove facts older than this
        
        Returns:
            Number of facts deleted
        """
        conn = sqlite3.connect(self.db_path)
        
        cursor = conn.execute("""
            DELETE FROM facts
            WHERE access_count < ?
              AND julianday('now') - julianday(verified_at) > ?
        """, (min_access_count, max_age_days))
        
        deleted = cursor.rowcount
        conn.commit()
        conn.close()
        
        if deleted > 0:
            logger.info(f"🧹 Cleaned up {deleted} low-value facts")
        
        return deleted
    
    def export_facts(self, output_path: str) -> int:
        """Export all facts to JSON file"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.execute("""
            SELECT question, answer, citations, confidence, 
                   verified_at, access_count, source, model
            FROM facts
            ORDER BY confidence DESC, access_count DESC
        """)
        
        facts = []
        for row in cursor:
            facts.append({
                "question": row[0],
                "answer": row[1],
                "citations": json.loads(row[2]),
                "confidence": row[3],
                "verified_at": row[4],
                "access_count": row[5],
                "source": row[6],
                "model": row[7]
            })
        
        conn.close()
        
        with open(output_path, 'w') as f:
            json.dump(facts, f, indent=2)
        
        logger.info(f"✅ Exported {len(facts)} facts to {output_path}")
        return len(facts)

