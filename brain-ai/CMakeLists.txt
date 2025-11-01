cmake_minimum_required(VERSION 3.15)
project(BrainAI VERSION 4.3.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -DNDEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra -Wpedantic")
endif()

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_GRPC_SERVICE "Build gRPC service" ON)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(USE_SANITIZERS "Enable address and undefined sanitizers" ON)

# Sanitizers (for development/CI)
if(USE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

# Find packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Fetch dependencies (header-only libraries)
include(FetchContent)

# nlohmann_json for JSON support
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# hnswlib for vector search (pinned commit for reproducibility)
# Pinned to commit 54ba206166f6a4d30b06d938753c75b6b036c6aa after v0.7.0 to ensure compatibility with C++17 and fix issue #123 (see https://github.com/nmslib/hnswlib/issues/123)
set(HNSWLIB_GIT_TAG 54ba206166f6a4d30b06d938753c75b6b036c6aa CACHE STRING "Pinned hnswlib commit")
FetchContent_Declare(
    hnswlib
    GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
    GIT_TAG        ${HNSWLIB_GIT_TAG}
    GIT_SHALLOW    FALSE
)
FetchContent_MakeAvailable(hnswlib)

# cpp-httplib for HTTP client
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

# pybind11 for Python bindings
if(BUILD_PYTHON_BINDINGS)
    find_package(pybind11 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        message(STATUS "pybind11 not found, fetching...")
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG        v2.11.1
        )
        FetchContent_MakeAvailable(pybind11)
        set(pybind11_FOUND TRUE)  # Mark as found after fetch
    endif()
endif()

# Verify required source files exist to prevent opaque build errors
foreach(_src
    src/monitoring/metrics.cpp
    src/monitoring/health.cpp
    src/logging/logger.cpp
    src/resilience/circuit_breaker.cpp
)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${_src}")
        message(FATAL_ERROR "Required source file not found: ${_src}")
    endif()
endforeach()

# Source files
set(BRAIN_AI_SOURCES
    # Core components
    src/utils.cpp
    src/episodic_buffer.cpp
    src/semantic_network.cpp
    src/hallucination_detector.cpp
    src/hybrid_fusion.cpp
    src/explanation_engine.cpp
    src/cognitive_handler.cpp
    
    # Production infrastructure (v4.0.1)
    src/monitoring/metrics.cpp
    src/monitoring/health.cpp
    src/logging/logger.cpp
    src/resilience/circuit_breaker.cpp
    
    # Vector search integration (v4.1.0)
    src/vector_search/hnsw_index.cpp
    
    # Document processing pipeline (v4.2.0 - DeepSeek-OCR integration)
    src/document/ocr_client.cpp
    src/document/text_validator.cpp
    src/document/document_processor.cpp
    
    # Enhanced indexing (v4.3.0 - Phase 5)
    src/indexing/index_manager.cpp
)

# Create library
add_library(brain_ai_lib STATIC ${BRAIN_AI_SOURCES})
target_link_libraries(brain_ai_lib
    PUBLIC
        Threads::Threads
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)
target_include_directories(brain_ai_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${hnswlib_SOURCE_DIR}
        ${httplib_SOURCE_DIR}
)
if(TARGET hnswlib)
    target_link_libraries(brain_ai_lib PRIVATE hnswlib)
endif()

# Main executable (demo)
add_executable(brain_ai_demo src/main.cpp)
target_link_libraries(brain_ai_demo PRIVATE brain_ai_lib)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Python bindings (pybind11)
if(BUILD_PYTHON_BINDINGS AND pybind11_FOUND)
    pybind11_add_module(brain_ai_py bindings/brain_ai_bindings.cpp)
    target_link_libraries(brain_ai_py PRIVATE brain_ai_lib)
    message(STATUS "✅ Python bindings enabled")
    
    pybind11_add_module(brain_ai_core bindings/pybind_module.cpp)
    target_link_libraries(brain_ai_core PRIVATE brain_ai_lib)
    set_target_properties(brain_ai_core PROPERTIES
        OUTPUT_NAME "brain_ai_core"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
    )
    message(STATUS "✅ brain_ai_core pybind module enabled")

    # Install Python module
    install(TARGETS brain_ai_py brain_ai_core
            LIBRARY DESTINATION .
            RUNTIME DESTINATION .
            ARCHIVE DESTINATION .)
endif()

# gRPC service (optional)
if(BUILD_GRPC_SERVICE)
    find_package(Protobuf)
    find_package(gRPC)
    if(Protobuf_FOUND AND gRPC_FOUND)
        add_subdirectory(proto)
    else()
        message(STATUS "Protobuf or gRPC not found - skipping gRPC service")
        set(BUILD_GRPC_SERVICE OFF)
    endif()
endif()

# Install targets
install(TARGETS brain_ai_lib brain_ai_demo
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY include/ DESTINATION include)
