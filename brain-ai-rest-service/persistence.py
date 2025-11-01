"""
Persistence layer for RAG++ system
Handles saving/loading of vector index and facts store
"""

import os
import json
import logging
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path
from typing import Dict, Any

try:
    import fcntl  # type: ignore
except ImportError:  # pragma: no cover - non-POSIX platforms
    fcntl = None

logger = logging.getLogger(__name__)


class PersistenceManager:
    """Manages persistence for vector index and system state"""
    
    def __init__(self, data_dir: str = "data/persistence"):
        self.data_dir = Path(data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)
        
        self.vector_index_path = self.data_dir / "vector_index.bin"
        self.metadata_path = self.data_dir / "metadata.json"
        self.lock_path = self.data_dir / ".brain_ai.lock"
        
        logger.info(f"✅ Persistence manager initialized (dir={self.data_dir})")
    
    @contextmanager
    def _lock(self, exclusive: bool = True):
        """Advisory filesystem lock to guard persistence operations."""
        if fcntl is None:
            yield
            return

        self.lock_path.parent.mkdir(parents=True, exist_ok=True)
        with open(self.lock_path, "w+") as lock_file:
            mode = fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH
            fcntl.flock(lock_file.fileno(), mode)
            try:
                yield
            finally:
                fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)

    def _write_json_atomic(self, path: Path, payload: Dict[str, Any]) -> None:
        """Write JSON to disk atomically using a temporary file."""
        tmp_path = path.with_suffix(path.suffix + ".tmp")
        with open(tmp_path, "w") as handle:
            json.dump(payload, handle, indent=2, sort_keys=True)
            handle.flush()
            os.fsync(handle.fileno())

        os.replace(tmp_path, path)
        try:
            dir_fd = os.open(str(path.parent), os.O_DIRECTORY)
        except (AttributeError, FileNotFoundError, NotADirectoryError, PermissionError):
            dir_fd = None
        if dir_fd is not None:
            try:
                os.fsync(dir_fd)
            finally:
                os.close(dir_fd)

    def _load_metadata(self) -> Dict[str, Any]:
        if not self.metadata_path.exists():
            return {}
        try:
            with open(self.metadata_path, "r") as handle:
                return json.load(handle)
        except Exception as exc:
            logger.error(f"Failed to read metadata: {exc}")
            return {}

    def save_vector_index(self, cognitive_handler) -> bool:
        """Save vector index to disk"""
        try:
            with self._lock(exclusive=True):
                tmp_index_path = self.vector_index_path.with_suffix(self.vector_index_path.suffix + ".tmp")
                success = cognitive_handler.vector_index().save(str(tmp_index_path))

                if not success:
                    if tmp_index_path.exists():
                        try:
                            tmp_index_path.unlink()
                        except OSError:
                            pass
                    return False

                os.replace(tmp_index_path, self.vector_index_path)

                metadata = {
                    "version": "1.0.0",
                    "schema_version": 1,
                    "format": "hnsw",
                    "vector_index_size": cognitive_handler.vector_index_size(),
                    "episodic_buffer_size": cognitive_handler.episodic_buffer_size(),
                    "semantic_network_size": cognitive_handler.semantic_network_size(),
                    "embedding_dim": cognitive_handler.vector_index().dimension() if hasattr(cognitive_handler.vector_index(), 'dimension') else 768,
                    "updated_at": datetime.utcnow().isoformat() + "Z",
                    "data_directory": str(self.data_dir),
                    "index_file": self.vector_index_path.name,
                }

                self._write_json_atomic(self.metadata_path, metadata)

                logger.info(
                    "✅ Saved vector index (%d docs, episodic=%d, semantic=%d)",
                    metadata["vector_index_size"],
                    metadata["episodic_buffer_size"],
                    metadata["semantic_network_size"]
                )
                return True

        except Exception as e:
            logger.error(f"Failed to save vector index: {e}")
            return False
    
    def load_vector_index(self, cognitive_handler) -> bool:
        """Load vector index from disk"""
        try:
            if not self.vector_index_path.exists():
                logger.info("No saved vector index found")
                return False

            metadata = self._load_metadata()
            if metadata:
                if metadata.get("schema_version") != 1:
                    logger.warning(
                        "Persistence metadata schema mismatch (expected=1, found=%s)",
                        metadata.get("schema_version")
                    )
                else:
                    logger.info(
                        "Loading vector index (version=%s, docs=%s)",
                        metadata.get("version"),
                        metadata.get("vector_index_size")
                    )
            else:
                logger.warning("No metadata found, attempting load anyway")

            with self._lock(exclusive=False):
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
        metadata = self._load_metadata()
        if not metadata:
            return {"exists": False}

        try:
            metadata["exists"] = True
            metadata["vector_index_file"] = str(self.vector_index_path)
            metadata["vector_index_size_mb"] = (
                self.vector_index_path.stat().st_size / (1024 * 1024)
                if self.vector_index_path.exists()
                else 0
            )
            metadata["lock_file"] = str(self.lock_path)
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

