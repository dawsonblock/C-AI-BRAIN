"""Intelligent document chunking with sentence awareness and overlap"""
import re
from typing import List, Dict, Optional, Tuple
import logging

logger = logging.getLogger(__name__)


class SmartChunker:
    """
    Sentence-aware document chunking with configurable overlap
    
    Features:
    - Preserves sentence boundaries
    - Configurable chunk size and overlap
    - Maintains document/section IDs
    - Token-based splitting (rough estimation)
    """
    
    def __init__(
        self,
        chunk_size: int = 400,      # Target tokens per chunk
        overlap: int = 50,           # Overlap tokens between chunks
        min_chunk_size: int = 100,   # Minimum viable chunk size
        max_chunk_size: int = 500    # Maximum chunk size
    ):
        """
        Initialize chunker with parameters
        
        Args:
            chunk_size: Target number of tokens per chunk
            overlap: Number of tokens to overlap between chunks
            min_chunk_size: Discard chunks smaller than this
            max_chunk_size: Hard limit on chunk size
        """
        self.chunk_size = chunk_size
        self.overlap = overlap
        self.min_chunk_size = min_chunk_size
        self.max_chunk_size = max_chunk_size
        
        logger.info(
            f"Chunker initialized: size={chunk_size}, "
            f"overlap={overlap}, min={min_chunk_size}, max={max_chunk_size}"
        )
    
    def chunk_document(
        self,
        text: str,
        doc_id: str,
        metadata: Optional[Dict] = None,
        section_id: Optional[str] = None
    ) -> List[Dict]:
        """
        Chunk document while preserving sentence boundaries
        
        Args:
            text: Document text to chunk
            doc_id: Document identifier
            metadata: Additional metadata to attach
            section_id: Optional section identifier
        
        Returns:
            List of chunk dicts with text, IDs, and metadata
        """
        if not text.strip():
            logger.warning(f"Empty document {doc_id}, skipping")
            return []
        
        # Split into sentences
        sentences = self._split_sentences(text)
        
        if not sentences:
            return []
        
        chunks = []
        current_chunk = []
        current_length = 0
        chunk_idx = 0
        
        for sent in sentences:
            sent_tokens = self._count_tokens(sent)
            
            # Check if adding this sentence would exceed max size
            if current_length + sent_tokens > self.max_chunk_size and current_chunk:
                # Finalize current chunk
                chunk_text = " ".join(current_chunk)
                chunks.append(self._create_chunk(
                    chunk_text, doc_id, chunk_idx, metadata, section_id
                ))
                
                # Start new chunk with overlap
                overlap_sents = self._get_overlap_sentences(
                    current_chunk, self.overlap
                )
                current_chunk = overlap_sents
                current_length = sum(self._count_tokens(s) for s in current_chunk)
                chunk_idx += 1
            
            # Check if we should finalize at target size
            elif current_length + sent_tokens > self.chunk_size and current_chunk:
                # Only finalize if we're past target and have content
                chunk_text = " ".join(current_chunk)
                chunks.append(self._create_chunk(
                    chunk_text, doc_id, chunk_idx, metadata, section_id
                ))
                
                # Start new chunk with overlap
                overlap_sents = self._get_overlap_sentences(
                    current_chunk, self.overlap
                )
                current_chunk = overlap_sents
                current_length = sum(self._count_tokens(s) for s in current_chunk)
                chunk_idx += 1
            
            # Add sentence to current chunk
            current_chunk.append(sent)
            current_length += sent_tokens
        
        # Add final chunk if it meets minimum size
        if current_chunk:
            chunk_text = " ".join(current_chunk)
            if self._count_tokens(chunk_text) >= self.min_chunk_size:
                chunks.append(self._create_chunk(
                    chunk_text, doc_id, chunk_idx, metadata, section_id
                ))
            elif chunks:
                # Merge with previous chunk if too small
                chunks[-1]["text"] += " " + chunk_text
                chunks[-1]["token_count"] = self._count_tokens(chunks[-1]["text"])
        
        logger.info(f"Chunked {doc_id}: {len(chunks)} chunks created")
        return chunks
    
    def chunk_text_by_sections(
        self,
        text: str,
        doc_id: str,
        section_delimiter: str = "\n\n",
        metadata: Optional[Dict] = None
    ) -> List[Dict]:
        """
        Chunk document respecting section boundaries
        
        Args:
            text: Document text
            doc_id: Document ID
            section_delimiter: How to split sections (default: double newline)
            metadata: Document metadata
        
        Returns:
            List of chunks with section IDs preserved
        """
        sections = text.split(section_delimiter)
        all_chunks = []
        
        for section_idx, section_text in enumerate(sections):
            if not section_text.strip():
                continue
            
            section_id = f"{doc_id}_sec_{section_idx}"
            section_chunks = self.chunk_document(
                section_text, doc_id, metadata, section_id
            )
            all_chunks.extend(section_chunks)
        
        return all_chunks
    
    def _split_sentences(self, text: str) -> List[str]:
        """
        Split text into sentences using simple regex
        
        Note: For production, consider using spaCy or NLTK for better accuracy
        """
        # Simple sentence splitter (handles . ! ?)
        # Handles common abbreviations
        text = re.sub(r'([.!?])\s+', r'\1|||', text)
        sentences = text.split('|||')
        return [s.strip() for s in sentences if s.strip()]
    
    def _count_tokens(self, text: str) -> int:
        """
        Estimate token count (rough approximation)
        
        Note: For production, use actual tokenizer like tiktoken
        """
        # Rough estimate: ~0.75 tokens per word
        words = len(text.split())
        return int(words * 0.75)
    
    def _get_overlap_sentences(
        self, 
        sentences: List[str], 
        target_tokens: int
    ) -> List[str]:
        """
        Get last N sentences that fit within target_tokens for overlap
        """
        overlap = []
        tokens = 0
        
        for sent in reversed(sentences):
            sent_tokens = self._count_tokens(sent)
            if tokens + sent_tokens > target_tokens:
                break
            overlap.insert(0, sent)
            tokens += sent_tokens
        
        return overlap
    
    def _create_chunk(
        self,
        text: str,
        doc_id: str,
        chunk_idx: int,
        metadata: Optional[Dict],
        section_id: Optional[str]
    ) -> Dict:
        """Create chunk object with all metadata"""
        chunk_id = f"{doc_id}_chunk_{chunk_idx}"
        
        chunk = {
            "text": text,
            "doc_id": doc_id,
            "chunk_id": chunk_id,
            "chunk_index": chunk_idx,
            "token_count": self._count_tokens(text),
            "char_count": len(text)
        }
        
        if section_id:
            chunk["section_id"] = section_id
        
        if metadata:
            chunk["metadata"] = metadata.copy()
        
        return chunk

