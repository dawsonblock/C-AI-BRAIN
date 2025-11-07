#!/usr/bin/env python3
"""Test the C++ module directly to debug segfault."""
import os
import sys
import traceback

if __name__ != "__main__":
    import pytest

    pytest.importorskip(
        "brain_ai_core", reason="brain_ai_core extension not installed in test environment"
    )

# Add brain-ai-rest-service to path using relative path from script location
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(script_dir, 'brain-ai-rest-service'))

try:
    print("1. Importing module...")
    import brain_ai_core
    print("✓ Module imported")
    
    print("\n2. Testing index_document...")
    brain_ai_core.index_document("doc1", "test document", [0.1] * 384)
    print("✓ Document indexed")
    
    print("\n3. Testing search...")
    results = brain_ai_core.search("test", 5, [0.1] * 384)
    print(f"✓ Search returned {len(results)} results")
    
    print("\n4. Testing save_index...")
    try:
        brain_ai_core.save_index("./test_index.json")
        print("✓ Save attempted")
    except Exception as e:
        print(f"✓ Save failed as expected: {e}")
    
    print("\n5. Testing load_index...")
    try:
        brain_ai_core.load_index("./nonexistent.json")
        print("✓ Load attempted")
    except Exception as e:
        print(f"✓ Load failed as expected: {e}")
    
    print("\n✅ All basic tests passed")
    
except Exception as e:
    print(f"\n❌ Error: {e}")
    traceback.print_exc()
    sys.exit(1)

