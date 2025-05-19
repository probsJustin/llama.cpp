#!/bin/bash
set -e

# Script to build and run llama.cpp with OpenTelemetry instrumentation

function print_section() {
    echo "==============================================="
    echo "$1"
    echo "==============================================="
}

# Check if a model path is provided
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <path_to_model.gguf> [--docker]"
    echo "Example: $0 models/llama-7b-q4_0.gguf"
    echo "Example with Docker: $0 models/llama-7b-q4_0.gguf --docker"
    exit 1
fi

MODEL_PATH="$1"
USE_DOCKER=false

# Check for --docker flag
if [ "$#" -gt 1 ]; then
    if [ "$2" == "--docker" ]; then
        USE_DOCKER=true
    fi
fi

# Get the absolute path of the model
if [[ ! "$MODEL_PATH" = /* ]]; then
    MODEL_PATH="$(pwd)/$MODEL_PATH"
fi

# Get the model filename only
MODEL_FILENAME=$(basename "$MODEL_PATH")

if [ "$USE_DOCKER" = true ]; then
    print_section "Building and running with Docker"
    
    # Build the Docker image
    print_section "Building the Docker image"
    docker-compose -f observability/dockerfiles/docker-compose.yaml build llama-cpp
    
    # Start the observability stack
    print_section "Starting the observability stack"
    docker-compose -f observability/dockerfiles/docker-compose.yaml up -d jaeger otel-collector prometheus grafana
    
    # Wait for services to start
    echo "Waiting for services to start..."
    sleep 5
    
    # Run the llama.cpp server
    print_section "Running the llama.cpp server with OpenTelemetry"
    docker-compose -f observability/dockerfiles/docker-compose.yaml run --rm -p 8080:8080 \
        -v "$MODEL_PATH:/app/models/$MODEL_FILENAME" \
        llama-cpp \
        /app/server -m "/app/models/$MODEL_FILENAME" --host 0.0.0.0 --port 8080
else
    print_section "Building and running locally"
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Build with OpenTelemetry support
    print_section "Building llama.cpp with OpenTelemetry support"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DLLAMA_NATIVE=ON -DLLAMA_BUILD_SERVER=ON -DLLAMA_OTEL_ENABLE=ON
    cmake --build . -j$(nproc)
    
    # Make the server executable available
    cp bin/server ../
    
    # Return to the root directory
    cd ..
    
    # Set environment variables for OpenTelemetry
    export OTEL_SERVICE_NAME=llama-cpp-server
    export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317
    export OTEL_RESOURCE_ATTRIBUTES=service.name=llama-cpp-server,service.version=1.0.0
    export OTEL_TRACES_SAMPLER=always_on
    
    # Start Jaeger (if not already running)
    if ! docker ps | grep -q jaeger; then
        print_section "Starting Jaeger"
        docker run -d --name jaeger \
            -e COLLECTOR_OTLP_ENABLED=true \
            -p 16686:16686 \
            -p 4317:4317 \
            -p 4318:4318 \
            jaegertracing/all-in-one:latest
        
        # Wait for Jaeger to start
        echo "Waiting for Jaeger to start..."
        sleep 5
    fi
    
    # Run the server
    print_section "Running llama.cpp server with OpenTelemetry"
    ./server -m "$MODEL_PATH" --host 0.0.0.0 --port 8080
fi

print_section "Server started! Access it at http://localhost:8080"
echo "Jaeger UI is available at http://localhost:16686"