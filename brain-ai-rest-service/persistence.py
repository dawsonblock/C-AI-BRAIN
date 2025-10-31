"""
Persistence layer for RAG++ system
Handles saving/loading of vector index and facts store
"""

import os
import json
import logging
from pathlib import Path
from typing import Dict, Any

logger = logging.getLogger(__name__)


class PersistenceManager:
    """Manages persistence for vector index and system state"""
    
    def __init__(self, data_dir: str = "data/persistence"):
        self.data_dir = Path(data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)
        
        self.vector_index_path = self.data_dir / "vector_index.bin"
        self.metadata_path = self.data_dir / "metadata.json"
        
        logger.info(f"✅ Persistence manager initialized (dir={self.data_dir})")
    
    def save_vector_index(self, cognitive_handler) -> bool:
        """Save vector index to disk"""
        try:
            # Save the HNSW index
            success = cognitive_handler.vector_index().save(str(self.vector_index_path))
            
            if success:
                # Save metadata
                metadata = {
                    "version": "1.0.0",
                    "vector_index_size": cognitive_handler.vector_index_size(),
                    "episodic_buffer_size": cognitive_handler.episodic_buffer_size(),
                    "semantic_network_size": cognitive_handler.semantic_network_size(),
                    "embedding_dim": cognitive_handler.vector_index().dimension() if hasattr(cognitive_handler.vector_index(), 'dimension') else 768
                }
                
                with open(self.metadata_path, 'w') as f:
                    json.dump(metadata, f, indent=2)
                
                logger.info(f"✅ Saved vector index ({metadata['vector_index_size']} docs)")
                return True
            
            return False
            
        except Exception as e:
            logger.error(f"Failed to save vector index: {e}")
            return False
    
    def load_vector_index(self, cognitive_handler) -> bool:
        """Load vector index from disk"""
        try:
            if not self.vector_index_path.exists():
                logger.info("No saved vector index found")
                return False
            
            if not self.metadata_path.exists():
                logger.warning("No metadata found, attempting load anyway")
            else:
                with open(self.metadata_path, 'r') as f:
                    metadata = json.load(f)
                logger.info(f"Loading vector index (version={metadata.get('version')}, docs={metadata.get('vector_index_size')})")
            
            # Load the HNSW index
            success = cognitive_handler.vector_index().load(str(self.vector_index_path))
            
            if success:
                logger.info(f"✅ Loaded vector index ({cognitive_handler.vector_index_size()} docs)")
                return True
            
            return False
            
        except Exception as e:
            logger.error(f"Failed to load vector index: {e}")
            return False
    
    def save_all(self, cognitive_handler, facts_store=None) -> Dict[str, bool]:
        """Save all persistent data"""
        results = {
            "vector_index": self.save_vector_index(cognitive_handler)
        }
        
        # Facts store saves automatically to SQLite, just log stats
        if facts_store:
            stats = facts_store.get_stats()
            logger.info(f"Facts store: {stats['total_facts']} facts cached")
        
        return results
    
    def load_all(self, cognitive_handler) -> Dict[str, bool]:
        """Load all persistent data"""
        results = {
            "vector_index": self.load_vector_index(cognitive_handler)
        }
        
        return results
    
    def exists(self) -> bool:
        """Check if saved data exists"""
        return self.vector_index_path.exists()
    
    def get_info(self) -> Dict[str, Any]:
        """Get information about saved data"""
        if not self.metadata_path.exists():
            return {"exists": False}
        
        try:
            with open(self.metadata_path, 'r') as f:
                metadata = json.load(f)
            
            metadata["exists"] = True
            metadata["vector_index_file"] = str(self.vector_index_path)
            metadata["vector_index_size_mb"] = self.vector_index_path.stat().st_size / (1024 * 1024) if self.vector_index_path.exists() else 0
            
            return metadata
        except Exception as e:
            logger.error(f"Failed to get persistence info: {e}")
            return {"exists": False, "error": str(e)}


class AutoSaveMixin:
    """Mixin to add auto-save functionality"""
    
    def __init__(self, persistence_manager: PersistenceManager, save_interval_docs: int = 10):
        self.persistence_manager = persistence_manager
        self.save_interval_docs = save_interval_docs
        self.docs_since_save = 0
    
    def maybe_save(self, cognitive_handler):
        """Save if threshold reached"""
        self.docs_since_save += 1
        
        if self.docs_since_save >= self.save_interval_docs:
            logger.info(f"Auto-saving after {self.docs_since_save} documents")
            self.persistence_manager.save_all(cognitive_handler)
            self.docs_since_save = 0

