# OpenTelemetry Instrumentation Plan for llama.cpp

This document outlines the plan for adding OpenTelemetry (OTEL) instrumentation to llama.cpp and setting up a Jaeger environment for distributed tracing.

## 1. Implementation Overview

We'll be adding OpenTelemetry instrumentation to llama.cpp to enable detailed tracing and metrics collection. The instrumented data will be sent to a Jaeger collector for visualization and analysis.

## 2. Docker Environment Setup

We'll create a Docker Compose environment with the following services:
- **llama-cpp**: The instrumented llama.cpp service
- **jaeger-all-in-one**: Jaeger service for tracing visualization
- **otel-collector**: OpenTelemetry collector for trace processing

## 3. Code Changes

### Core Instrumentation Changes

1. **OpenTelemetry C++ SDK Integration**
   - Add OpenTelemetry C++ SDK as a dependency
   - Create a CMake option for enabling/disabling OTEL instrumentation
   - Implement a minimal wrapper layer for OTEL tracing

2. **Core API Instrumentation**
   - File: `/src/llama.cpp`
   - Add tracing to key functions:
     - `llama_decode`
     - `llama_model_load`
     - Token generation and sampling
   - Link: [src/llama.cpp](https://github.com/ggerganov/llama.cpp/blob/master/src/llama.cpp)

3. **Server Instrumentation**
   - File: `/tools/server/server.cpp`
   - Add middleware for HTTP request tracing
   - Trace request handling from input to response
   - Track different request types (completion, chat)
   - Link: [tools/server/server.cpp](https://github.com/ggerganov/llama.cpp/blob/master/tools/server/server.cpp)

4. **CLI Tool Instrumentation**
   - File: `/tools/main/main.cpp`
   - Add tracing to key processing steps
   - Measure model loading and inference time
   - Link: [tools/main/main.cpp](https://github.com/ggerganov/llama.cpp/blob/master/tools/main/main.cpp)

### Implementation Details

1. **OTEL Wrapper Layer**
   - Create new files: `/common/otel.h` and `/common/otel.cpp`
   - Implement conditional compilation for OTEL support
   - Provide simple macros for span creation and attribute setting

2. **Context Propagation**
   - Modify `llama_context` to include OTEL context
   - Ensure trace context is propagated through the call chain

3. **Metrics Collection**
   - Add counters for tokens processed
   - Add histograms for token generation time
   - Track memory usage metrics

4. **Build System Changes**
   - Update CMakeLists.txt to include OpenTelemetry dependencies
   - Add OTEL_ENABLED build option

## 4. Implementation Steps

1. Create Docker environment for testing
2. Add OpenTelemetry C++ SDK integration
3. Implement basic OTEL wrapper layer
4. Instrument server component
5. Instrument core inference functions
6. Add detailed spans for performance-critical sections
7. Implement metrics collection
8. Test and validate tracing data in Jaeger

## 5. Testing Strategy

1. Create a Docker Compose setup for testing
2. Verify trace propagation across different components
3. Test with varying load to ensure performance impact is minimal
4. Validate that all key operations are properly traced
5. Ensure that traces contain sufficient detail for debugging

## 6. Expected Outcomes

After implementation, we should be able to:
1. Visualize the complete request flow through llama.cpp
2. Identify performance bottlenecks in the inference pipeline
3. Track token generation latency and throughput
4. Monitor memory usage patterns
5. Analyze sampling algorithm performance

## 7. Future Enhancements

1. Add more detailed instrumentation to computation-intensive functions
2. Implement custom metrics for model-specific performance characteristics
3. Add dashboard templates for common monitoring scenarios
4. Extend instrumentation to other components (quantization, training)