"""Tests for database module."""

import pytest
import tempfile
import sqlite3
from pathlib import Path
from app.database import (
    initialize_database,
    get_connection,
    get_db_connection,
    DatabasePool,
)


@pytest.fixture
def temp_db():
    """Create temporary database for testing."""
    with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
        db_path = f.name
    yield db_path
    # Cleanup
    Path(db_path).unlink(missing_ok=True)
    Path(f"{db_path}-shm").unlink(missing_ok=True)
    Path(f"{db_path}-wal").unlink(missing_ok=True)


class TestDatabaseInitialization:
    """Test database initialization."""
    
    def test_initialize_creates_tables(self, temp_db):
        """Test that initialization creates required tables."""
        initialize_database(temp_db)
        
        conn = sqlite3.connect(temp_db)
        cursor = conn.cursor()
        
        # Check documents table exists
        cursor.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='documents'"
        )
        assert cursor.fetchone() is not None
        
        # Check query_cache table exists
        cursor.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='query_cache'"
        )
        assert cursor.fetchone() is not None
        
        conn.close()
    
    def test_initialize_creates_indexes(self, temp_db):
        """Test that indexes are created."""
        initialize_database(temp_db)
        
        conn = sqlite3.connect(temp_db)
        cursor = conn.cursor()
        
        cursor.execute(
            "SELECT name FROM sqlite_master WHERE type='index'"
        )
        indexes = [row[0] for row in cursor.fetchall()]
        
        assert any("documents" in idx for idx in indexes)
        assert any("query_cache" in idx for idx in indexes)
        
        conn.close()


class TestDatabaseConnection:
    """Test database connection management."""
    
    def test_connection_with_wal_mode(self, temp_db):
        """Test connection enables WAL mode."""
        initialize_database(temp_db)
        conn = get_connection(temp_db)
        
        cursor = conn.cursor()
        cursor.execute("PRAGMA journal_mode")
        mode = cursor.fetchone()[0]
        
        assert mode.upper() == "WAL"
        conn.close()
    
    def test_connection_timeout_set(self, temp_db):
        """Test connection has timeout configured."""
        initialize_database(temp_db)
        conn = get_connection(temp_db)
        
        cursor = conn.cursor()
        cursor.execute("PRAGMA busy_timeout")
        timeout = cursor.fetchone()[0]
        
        assert timeout > 0
        conn.close()
    
    def test_context_manager(self, temp_db):
        """Test database context manager."""
        initialize_database(temp_db)
        
        with get_db_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT 1")
            result = cursor.fetchone()
            assert result[0] == 1


class TestDatabasePool:
    """Test connection pooling."""
    
    def test_pool_creation(self, temp_db):
        """Test pool can be created."""
        initialize_database(temp_db)
        pool = DatabasePool(temp_db, pool_size=3)
        assert pool.pool_size == 3
        assert len(pool._connections) == 0
    
    def test_pool_get_connection(self, temp_db):
        """Test getting connection from pool."""
        initialize_database(temp_db)
        pool = DatabasePool(temp_db, pool_size=3)
        
        conn1 = pool.get_connection()
        assert conn1 is not None
        assert len(pool._connections) == 1
        assert len(pool._in_use) == 1
    
    def test_pool_reuse_connection(self, temp_db):
        """Test connection reuse."""
        initialize_database(temp_db)
        pool = DatabasePool(temp_db, pool_size=3)
        
        conn1 = pool.get_connection()
        pool.release_connection(conn1)
        
        conn2 = pool.get_connection()
        assert conn1 is conn2  # Same connection object
    
    def test_pool_size_limit(self, temp_db):
        """Test pool respects size limit."""
        initialize_database(temp_db)
        pool = DatabasePool(temp_db, pool_size=2)
        
        conn1 = pool.get_connection()
        conn2 = pool.get_connection()
        
        assert len(pool._connections) == 2
        
        # Third connection should be temporary (not in pool)
        conn3 = pool.get_connection()
        assert len(pool._connections) == 2  # Still only 2 in pool
    
    def test_pool_close_all(self, temp_db):
        """Test closing all connections."""
        initialize_database(temp_db)
        pool = DatabasePool(temp_db, pool_size=3)
        
        conn1 = pool.get_connection()
        conn2 = pool.get_connection()
        
        pool.close_all()
        
        assert len(pool._connections) == 0
        assert len(pool._in_use) == 0


class TestDatabaseOperations:
    """Test actual database operations."""
    
    def test_insert_document(self, temp_db):
        """Test inserting a document."""
        initialize_database(temp_db)
        
        with get_db_connection() as conn:
            cursor = conn.cursor()
            cursor.execute(
                "INSERT INTO documents (id, text, metadata) VALUES (?, ?, ?)",
                ("doc1", "Test document", '{"key": "value"}')
            )
            conn.commit()
            
            cursor.execute("SELECT * FROM documents WHERE id = ?", ("doc1",))
            row = cursor.fetchone()
            assert row is not None
            assert row[1] == "Test document"
    
    def test_query_cache(self, temp_db):
        """Test query cache operations."""
        initialize_database(temp_db)
        
        with get_db_connection() as conn:
            cursor = conn.cursor()
            cursor.execute(
                "INSERT INTO query_cache (query_hash, query, results) VALUES (?, ?, ?)",
                ("hash123", "test query", '{"results": []}')
            )
            conn.commit()
            
            cursor.execute("SELECT * FROM query_cache WHERE query_hash = ?", ("hash123",))
            row = cursor.fetchone()
            assert row is not None
            assert row[1] == "test query"
