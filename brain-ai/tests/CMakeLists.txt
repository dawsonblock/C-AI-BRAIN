cmake_minimum_required(VERSION 3.15)

# Find Google Test (if available)
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "Google Test not found - tests will use simple assertions")
    
    # Create simple test runner without GTest
    add_executable(brain_ai_tests
        test_utils.cpp
        test_episodic_buffer.cpp
        test_semantic_network.cpp
        test_hallucination_detector.cpp
        test_hybrid_fusion.cpp
        test_explanation_engine.cpp
        test_cognitive_handler.cpp
        test_main.cpp
    )
    
    # New tests for v4.0.1 infrastructure
    add_executable(brain_ai_monitoring_tests
        test_monitoring.cpp
    )
    
    add_executable(brain_ai_resilience_tests
        test_resilience.cpp
    )
    
    # New tests for v4.1.0 vector search
    add_executable(brain_ai_vector_search_tests
        test_vector_search.cpp
    )
    
    # New tests for v4.2.0 document processing
    add_executable(brain_ai_document_processor_tests
        test_document_processor.cpp
    )
    
    # Integration tests for v4.3.0 (Phase 3)
    add_executable(brain_ai_ocr_integration_tests
        integration/test_ocr_integration.cpp
    )
    
    target_link_libraries(brain_ai_tests PRIVATE brain_ai_lib)
    target_link_libraries(brain_ai_monitoring_tests PRIVATE brain_ai_lib)
    target_link_libraries(brain_ai_resilience_tests PRIVATE brain_ai_lib)
    target_link_libraries(brain_ai_vector_search_tests PRIVATE brain_ai_lib)
    target_link_libraries(brain_ai_document_processor_tests PRIVATE brain_ai_lib)
    target_link_libraries(brain_ai_ocr_integration_tests PRIVATE brain_ai_lib)
    
else()
    message(STATUS "Google Test found - building with GTest")
    
    # Create test with GTest
    add_executable(brain_ai_tests
        gtest_utils.cpp
        gtest_episodic_buffer.cpp
        gtest_semantic_network.cpp
        gtest_hallucination_detector.cpp
        gtest_hybrid_fusion.cpp
        gtest_explanation_engine.cpp
        gtest_cognitive_handler.cpp
    )
    
    target_link_libraries(brain_ai_tests PRIVATE 
        brain_ai_lib
        GTest::GTest
        GTest::Main
    )
endif()

# Add tests to CTest
add_test(NAME BrainAITests COMMAND brain_ai_tests)

# Always register the new infrastructure tests if the targets exist
if(TARGET brain_ai_monitoring_tests)
    add_test(NAME MonitoringTests COMMAND brain_ai_monitoring_tests)
endif()

if(TARGET brain_ai_resilience_tests)
    add_test(NAME ResilienceTests COMMAND brain_ai_resilience_tests)
    add_test(NAME VectorSearchTests COMMAND brain_ai_vector_search_tests)
endif()

if(TARGET brain_ai_document_processor_tests)
    add_test(NAME DocumentProcessorTests COMMAND brain_ai_document_processor_tests)
endif()

if(TARGET brain_ai_ocr_integration_tests)
    add_test(NAME OCRIntegrationTests COMMAND brain_ai_ocr_integration_tests)
endif()
