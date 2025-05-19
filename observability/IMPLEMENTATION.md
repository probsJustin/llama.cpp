# Implementing OpenTelemetry in llama.cpp

This document provides step-by-step instructions for implementing OpenTelemetry instrumentation in llama.cpp.

## Prerequisites

Before starting, ensure you have:

1. OpenTelemetry C++ SDK installed
2. Docker and Docker Compose installed
3. A clean build environment for llama.cpp

## Implementation Steps

### 1. Add OpenTelemetry Wrapper Layer

The OpenTelemetry wrapper layer consists of two files:
- `common/otel/otel.h`: Header with macro definitions
- `common/otel/otel.cpp`: Implementation of the OpenTelemetry API

These files provide a simple, conditionally-compiled interface for adding traces and metrics to the codebase.

### 2. Update CMake Configuration

Add the following to your `CMakeLists.txt`:

```cmake
# OpenTelemetry support
option(LLAMA_OTEL_ENABLE "llama: enable OpenTelemetry instrumentation" OFF)

if (LLAMA_OTEL_ENABLE)
    message(STATUS "OpenTelemetry support enabled")
    add_compile_definitions(LLAMA_OTEL_ENABLE)
endif()
```

Also update the `common/CMakeLists.txt` to include the OTEL files:

```cmake
# Add OpenTelemetry files to the common target
add_library(${TARGET} STATIC
    # Existing files...
    otel/otel.cpp
    otel/otel.h
    )

# OpenTelemetry support
if (LLAMA_OTEL_ENABLE)
    find_package(Protobuf REQUIRED)
    find_library(OPENTELEMETRY_API_LIBRARY opentelemetry_api REQUIRED)
    find_library(OPENTELEMETRY_SDK_LIBRARY opentelemetry_sdk REQUIRED)
    find_library(OPENTELEMETRY_OTLP_LIBRARY opentelemetry_exporter_otlp_grpc REQUIRED)

    target_compile_definitions(${TARGET} PUBLIC LLAMA_OTEL_ENABLE)
    set(LLAMA_COMMON_EXTRA_LIBS ${LLAMA_COMMON_EXTRA_LIBS} ${OPENTELEMETRY_API_LIBRARY} ${OPENTELEMETRY_SDK_LIBRARY} ${OPENTELEMETRY_OTLP_LIBRARY} ${PROTOBUF_LIBRARIES})
    target_include_directories(${TARGET} PRIVATE ${PROTOBUF_INCLUDE_DIRS})
endif()
```

### 3. Instrument Core Components

Instrument the following key areas:

1. **Model Loading**:
   - Add spans around `llama_model_load` function
   - Record model loading time and size metrics

2. **Token Generation**:
   - Add spans around `llama_decode` function
   - Record token generation time and count

3. **Server Request Handling**:
   - Add spans around HTTP request handling
   - Track request types and latency
   - Monitor queue depth and processing time

### 4. Set Up Docker Environment

Use the provided Docker Compose configuration to set up:
- Jaeger for tracing visualization
- OpenTelemetry Collector for telemetry processing
- Prometheus for metrics storage
- Grafana for dashboards

### 5. Building with OpenTelemetry Support

To build llama.cpp with OpenTelemetry support:

```bash
mkdir build && cd build
cmake .. -DLLAMA_OTEL_ENABLE=ON
make -j$(nproc)
```

Or use the provided Docker image:

```bash
docker-compose -f observability/dockerfiles/docker-compose.yaml build llama-cpp
```

### 6. Running with OpenTelemetry

Run llama.cpp server with OpenTelemetry:

```bash
# Set the OpenTelemetry endpoint
export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317

# Run the server
./server -m models/your-model.gguf --host 0.0.0.0 --port 8080
```

Or using Docker Compose:

```bash
docker-compose -f observability/dockerfiles/docker-compose.yaml up
```

## Viewing Telemetry Data

1. **Jaeger UI**: http://localhost:16686
   - View distributed traces
   - Search by service, operation, or tags
   - Analyze trace dependencies

2. **Grafana**: http://localhost:3000
   - View dashboards for token generation performance
   - Monitor memory usage and throughput
   - Track error rates

## Additional Instrumentation Areas

Consider adding instrumentation to these areas as well:

1. **Memory Management**:
   - Track KV cache usage
   - Monitor context window utilization

2. **Sampling Functions**:
   - Measure time spent in different sampling algorithms
   - Monitor parameter effects on performance

3. **Backend Operations**:
   - Track GPU/CPU operation times
   - Measure tensor computation performance