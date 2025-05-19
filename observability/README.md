# Observability for llama.cpp

This directory contains tools and configurations for adding observability to llama.cpp using OpenTelemetry and Jaeger.

## Overview

The observability stack includes:

1. **OpenTelemetry Instrumentation**: Added to llama.cpp to trace key functions and collect metrics
2. **Jaeger**: Distributed tracing system for visualization and analysis
3. **Prometheus**: Metrics collection and storage
4. **Grafana**: Dashboard visualization

## Directory Structure

- **`/dockerfiles`**: Docker and Docker Compose files for the observability stack
- **`/prompts`**: Planning documents and implementation notes
- **`/common/otel`**: OpenTelemetry wrapper code for llama.cpp

## Getting Started

### Option 1: Using Docker Compose

1. Start the Docker Compose stack:
   ```
   docker-compose -f observability/dockerfiles/docker-compose.yaml up
   ```

2. Access the Jaeger UI:
   ```
   http://localhost:16686
   ```

3. Access the Grafana dashboard:
   ```
   http://localhost:3000
   ```
   (Default credentials: admin/admin)

### Option 2: Run with helper script

1. Use the run_instrumented.sh script:
   ```
   ./observability/run_instrumented.sh /path/to/your/model.gguf
   ```

2. For Docker mode:
   ```
   ./observability/run_instrumented.sh /path/to/your/model.gguf --docker
   ```

### Option 3: Build manually with OpenTelemetry

1. Build llama.cpp with OpenTelemetry support:
   ```
   mkdir build && cd build
   cmake .. -DLLAMA_OTEL_ENABLE=ON
   make -j$(nproc)
   ```

2. Run with OpenTelemetry environment variables:
   ```
   export OTEL_SERVICE_NAME=llama-cpp
   export OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4317
   ./server -m /path/to/your/model.gguf
   ```

## Testing the Server

Use the provided test script to send completion requests:

```
python3 observability/test_completion.py --url http://localhost:8080 --prompt "Write a poem about AI"
```

For parallel requests (to test load):

```
python3 observability/test_completion.py --url http://localhost:8080 --parallel 5
```

## Instrumented Components

The following components are instrumented with OpenTelemetry:

1. **Core Model Operations**
   - Model loading
   - Context creation
   - Token decoding
   - Batch processing

2. **Server Operations**
   - HTTP request handling
   - Task queue management
   - Token generation
   - Response streaming

3. **Resource Usage**
   - Memory allocation and usage
   - Context window utilization
   - Generation throughput

## Implementation Details

See the [IMPLEMENTATION.md](IMPLEMENTATION.md) file for detailed implementation instructions and technical details.

## Future Enhancements

Planned improvements to the observability stack:

1. **Custom Sampling**: Implement smarter sampling for high-volume production deployments
2. **Exemplars**: Add support for linking metrics to traces with exemplars
3. **Health Checks**: Add health check instrumentation
4. **Resource Quotas**: Monitor and enforce resource quota limits
5. **Alarm Rules**: Add Prometheus AlertManager rules for common failure conditions