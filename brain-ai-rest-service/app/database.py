"""Database utilities with SQLite WAL mode and proper connection handling."""

import sqlite3
import logging
from contextlib import contextmanager
from typing import Generator
from pathlib import Path

from .config import settings

logger = logging.getLogger(__name__)


def initialize_database(db_path: str) -> None:
    """
    Initialize database with proper schema and settings.
    
    Args:
        db_path: Path to SQLite database file
    """
    # Ensure directory exists
    Path(db_path).parent.mkdir(parents=True, exist_ok=True)
    
    conn = get_connection(db_path)
    try:
        cursor = conn.cursor()
        
        # Create tables if they don't exist
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS documents (
                id TEXT PRIMARY KEY,
                text TEXT NOT NULL,
                embedding BLOB,
                metadata TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS query_cache (
                query_hash TEXT PRIMARY KEY,
                query TEXT NOT NULL,
                results TEXT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                accessed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                access_count INTEGER DEFAULT 1
            )
        """)
        
        # Create indexes
        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_documents_created 
            ON documents(created_at)
        """)
        
        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_query_cache_accessed 
            ON query_cache(accessed_at)
        """)
        
        conn.commit()
        logger.info(f"Database initialized: {db_path}")
        
    except sqlite3.Error as e:
        logger.error(f"Failed to initialize database: {e}")
        raise
    finally:
        conn.close()


def get_connection(db_path: str | None = None) -> sqlite3.Connection:
    """
    Create a SQLite connection with proper WAL mode and settings.
    
    Args:
        db_path: Path to database file, uses config default if None
        
    Returns:
        Configured SQLite connection
    """
    if db_path is None:
        db_path = settings.db_path
    
    try:
        conn = sqlite3.connect(
            db_path,
            check_same_thread=False,
            timeout=settings.db_timeout / 1000.0,  # Convert to seconds
            isolation_level=None  # Autocommit mode
        )
        
        # Enable WAL mode for better concurrency
        conn.execute("PRAGMA journal_mode=WAL;")
        
        # Set synchronous to NORMAL for better performance with WAL
        conn.execute("PRAGMA synchronous=NORMAL;")
        
        # Set busy timeout
        conn.execute(f"PRAGMA busy_timeout={settings.db_timeout};")
        
        # Enable foreign keys
        conn.execute("PRAGMA foreign_keys=ON;")
        
        # Set reasonable cache size (negative = KB)
        conn.execute("PRAGMA cache_size=-32000;")  # 32MB
        
        logger.debug(f"Database connection created: {db_path}")
        return conn
        
    except sqlite3.Error as e:
        logger.error(f"Failed to connect to database: {e}")
        raise


@contextmanager
def get_db_connection() -> Generator[sqlite3.Connection, None, None]:
    """
    Context manager for database connections.
    
    Yields:
        SQLite connection
        
    Example:
        with get_db_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM documents")
    """
    conn = get_connection()
    try:
        yield conn
    except sqlite3.Error as e:
        logger.error(f"Database error: {e}")
        raise
    finally:
        conn.close()


class DatabasePool:
    """Simple connection pool for SQLite."""
    
    def __init__(self, db_path: str, pool_size: int = 5):
        """
        Initialize connection pool.
        
        Args:
            db_path: Path to database file
            pool_size: Maximum number of connections
        """
        self.db_path = db_path
        self.pool_size = pool_size
        self._connections = []
        self._in_use = set()
    
    def get_connection(self) -> sqlite3.Connection:
        """Get a connection from the pool."""
        # Reuse idle connection if available
        for conn in self._connections:
            if conn not in self._in_use:
                self._in_use.add(conn)
                return conn
        
        # Create new connection if under pool size
        if len(self._connections) < self.pool_size:
            conn = get_connection(self.db_path)
            self._connections.append(conn)
            self._in_use.add(conn)
            return conn
        
        # Pool exhausted - create temporary connection
        logger.warning("Connection pool exhausted, creating temporary connection")
        return get_connection(self.db_path)
    
    def release_connection(self, conn: sqlite3.Connection) -> None:
        """Release a connection back to the pool."""
        if conn in self._in_use:
            self._in_use.remove(conn)
    
    def close_all(self) -> None:
        """Close all connections in the pool."""
        for conn in self._connections:
            try:
                conn.close()
            except sqlite3.Error as e:
                logger.error(f"Error closing connection: {e}")
        self._connections.clear()
        self._in_use.clear()


# Global connection pool instance
_db_pool = None


def get_db_pool() -> DatabasePool:
    """Get the global database pool instance."""
    global _db_pool
    if _db_pool is None:
        _db_pool = DatabasePool(settings.db_path, settings.db_pool_size)
    return _db_pool
