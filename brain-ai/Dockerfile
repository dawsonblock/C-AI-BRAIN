# Brain-AI v4.0 Production Dockerfile
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    git \
    wget \
    libssl-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY include/ /app/include/
COPY src/ /app/src/
COPY tests/ /app/tests/
COPY CMakeLists.txt /app/
COPY cmake/ /app/cmake/

# Create build directory
RUN mkdir -p /app/build

# Build project
WORKDIR /app/build
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Run tests (ENFORCED - build fails if tests fail)
RUN ctest --output-on-failure

# Create data directories
RUN mkdir -p /app/data/episodic /app/data/semantic

# Expose port for future gRPC service
EXPOSE 50051

# Set entrypoint
WORKDIR /app
ENTRYPOINT ["./build/brain_ai_demo"]

# Labels
LABEL maintainer="Brain-AI Team"
LABEL version="4.0.0"
LABEL description="Brain-AI v4.0 Production Cognitive Architecture"
