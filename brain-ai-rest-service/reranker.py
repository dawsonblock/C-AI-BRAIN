"""Cross-encoder re-ranker for retrieval results"""
from sentence_transformers import CrossEncoder
from typing import List, Tuple, Dict, Optional
import logging

logger = logging.getLogger(__name__)


class ReRanker:
    """Re-rank retrieval results using cross-encoder for better relevance"""
    
    def __init__(self, model_name: str = "cross-encoder/ms-marco-MiniLM-L-6-v2"):
        """
        Initialize re-ranker with cross-encoder model
        
        Available models:
        - cross-encoder/ms-marco-MiniLM-L-6-v2 (80MB, fast, good)
        - cross-encoder/ms-marco-MiniLM-L-12-v2 (120MB, slower, better)
        - cross-encoder/ms-marco-TinyBERT-L-2-v2 (17MB, fastest, decent)
        """
        logger.info(f"Loading re-ranker model: {model_name}")
        try:
            self.model = CrossEncoder(model_name, max_length=512)
            logger.info(f"✅ Re-ranker ready ({model_name})")
        except Exception as e:
            logger.error(f"Failed to load re-ranker: {e}")
            raise
    
    def rerank(
        self,
        query: str,
        documents: List[str],
        top_k: Optional[int] = None
    ) -> List[Tuple[int, float]]:
        """
        Re-rank documents by query relevance using cross-encoder
        
        Args:
            query: Query text
            documents: List of document texts
            top_k: Return top K results (None = all)
        
        Returns:
            List of (original_index, score) sorted by relevance score descending
        """
        if not documents:
            return []
        
        # Create query-document pairs
        pairs = [[query, doc] for doc in documents]
        
        # Score all pairs (cross-attention, slower but more accurate than bi-encoder)
        scores = self.model.predict(pairs)
        
        # Sort by score descending
        ranked = sorted(
            enumerate(scores),
            key=lambda x: x[1],
            reverse=True
        )
        
        # Return top-k if specified
        if top_k is not None:
            ranked = ranked[:top_k]
        
        logger.debug(f"Re-ranked {len(documents)} docs, returning top {len(ranked)}")
        return ranked
    
    def rerank_with_docs(
        self,
        query: str,
        documents: List[Dict],
        top_k: Optional[int] = None,
        content_key: str = "content"
    ) -> List[Dict]:
        """
        Re-rank document objects (dicts) by relevance
        
        Args:
            query: Query text
            documents: List of document dicts
            top_k: Return top K (None = all)
            content_key: Key in dict containing text to rank
        
        Returns:
            List of re-ranked document dicts with added 'rerank_score' field
        """
        if not documents:
            return []
        
        # Extract texts
        texts = [doc.get(content_key, "") for doc in documents]
        
        # Re-rank
        ranked_indices = self.rerank(query, texts, top_k)
        
        # Reconstruct documents with scores
        result = []
        for idx, score in ranked_indices:
            doc = documents[idx].copy()
            doc["rerank_score"] = float(score)
            doc["rerank_position"] = len(result) + 1
            result.append(doc)
        
        return result
    
    def get_scores(
        self,
        query: str,
        documents: List[str]
    ) -> List[float]:
        """
        Get relevance scores without re-ranking
        
        Returns:
            List of scores in original order
        """
        if not documents:
            return []
        
        pairs = [[query, doc] for doc in documents]
        scores = self.model.predict(pairs)
        
        return scores.tolist()

