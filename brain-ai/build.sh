#!/bin/bash
# Brain-AI v4.0 Build Script

set -e  # Exit on error

echo "========================================="
echo "Brain-AI v4.0 Build Script"
echo "========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Parse arguments
BUILD_TYPE="Release"
RUN_TESTS=true
RUN_DEMO=false
CLEAN=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-tests)
            RUN_TESTS=false
            shift
            ;;
        --demo)
            RUN_DEMO=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--debug] [--no-tests] [--demo] [--clean]"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
    echo -e "${GREEN}✓ Clean complete${NC}"
    echo ""
fi

# Create build directory
echo -e "${YELLOW}Creating build directory...${NC}"
mkdir -p build
cd build

# Configure
echo -e "${YELLOW}Configuring CMake (${BUILD_TYPE})...${NC}"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DBUILD_TESTS=ON \
      ..

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Configuration successful${NC}"
else
    echo -e "${RED}✗ Configuration failed${NC}"
    exit 1
fi
echo ""

# Build
echo -e "${YELLOW}Building project...${NC}"
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo ""

# Run tests
if [ "$RUN_TESTS" = true ]; then
    echo -e "${YELLOW}Running tests...${NC}"
    ctest --output-on-failure
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed${NC}"
    else
        echo -e "${RED}✗ Some tests failed${NC}"
        exit 1
    fi
    echo ""
fi

# Run demo
if [ "$RUN_DEMO" = true ]; then
    echo -e "${YELLOW}Running demo...${NC}"
    echo ""
    ./brain_ai_demo
    echo ""
fi

# Summary
echo "========================================="
echo -e "${GREEN}Build Complete!${NC}"
echo "========================================="
echo ""
echo "Executables:"
echo "  • Demo:  ./build/brain_ai_demo"
echo "  • Tests: ./build/tests/brain_ai_tests"
echo ""
echo "To run:"
echo "  cd build"
echo "  ./brain_ai_demo"
echo ""
echo "To test:"
echo "  cd build"
echo "  ctest --output-on-failure"
echo ""
echo "To install:"
echo "  cd build"
echo "  sudo make install"
echo ""
