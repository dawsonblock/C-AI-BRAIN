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
option(USE_SANITIZERS "Enable address and thread sanitizers" OFF)

# Sanitizers (for development)
if(USE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,thread,undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,thread,undefined")
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

# hnswlib for vector search
FetchContent_Declare(
    hnswlib
    GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
    GIT_TAG        v0.8.0
)
FetchContent_MakeAvailable(hnswlib)

# cpp-httplib for HTTP client
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${hnswlib_SOURCE_DIR})
include_directories(${httplib_SOURCE_DIR})

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
target_link_libraries(brain_ai_lib PUBLIC 
    Threads::Threads 
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Main executable (demo)
add_executable(brain_ai_demo src/main.cpp)
target_link_libraries(brain_ai_demo PRIVATE brain_ai_lib)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
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
