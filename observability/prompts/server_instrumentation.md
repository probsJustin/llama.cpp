# Server Instrumentation Plan for OpenTelemetry

## Key Areas for Instrumentation

1. **Request Handling**
   - Trace HTTP request processing from start to finish
   - Track request types (completion, embedding, rerank, etc.)
   - Measure request latency
   - Count requests by type

2. **Task Queue Management**
   - Trace task processing lifecycle
   - Track queue length and wait times
   - Measure time spent in different task states

3. **Context Management**
   - Track KV cache usage
   - Measure context loading/saving operations
   - Monitor context shifts and trimming

4. **Token Generation**
   - Measure time per token generation
   - Track batch sizes
   - Monitor sampling parameters

5. **Error Handling**
   - Track error rates by type
   - Capture error contexts and messages

## Implementation Approach

### 1. Add OpenTelemetry Headers

```cpp
#include "common/otel/otel.h"
```

### 2. Initialize OpenTelemetry in Main Function

Add initialization code to the main function:

```cpp
// Initialize OpenTelemetry if enabled
#if LLAMA_OTEL_ENABLE
    std::string service_name = "llama-cpp-server";
    std::string service_version = LLAMA_BUILD_NUMBER;
    std::string collector_endpoint = SERVER_OTEL_ENDPOINT;
    
    // Get endpoint from environment if available
    const char* env_endpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
    if (env_endpoint != nullptr) {
        collector_endpoint = env_endpoint;
    }
    
    llama::otel::Initialize(service_name, service_version, collector_endpoint);
    LOG_INFO("OpenTelemetry initialized with endpoint: %s", collector_endpoint.c_str());
#endif
```

### 3. Add Shutdown Handling

```cpp
// Cleanup OpenTelemetry on shutdown
#if LLAMA_OTEL_ENABLE
    llama::otel::Shutdown();
#endif
```

### 4. Instrument HTTP Request Handling

Add a span around HTTP request handling:

```cpp
static void http_request_handler(svr, req, res) {
    LLAMA_OTEL_SPAN("http_request");
    LLAMA_OTEL_ADD_ATTRIBUTE("http.method", req.method);
    LLAMA_OTEL_ADD_ATTRIBUTE("http.url", req.path);
    
    // Process request...
    
    LLAMA_OTEL_ADD_ATTRIBUTE("http.status_code", std::to_string(res.status));
}
```

### 5. Instrument Task Processing

Add spans around task processing:

```cpp
void process_task(server_task& task) {
    LLAMA_OTEL_SPAN("process_task");
    LLAMA_OTEL_ADD_ATTRIBUTE("task.type", task_type_to_string(task.type));
    LLAMA_OTEL_ADD_ATTRIBUTE("task.id", std::to_string(task.id));
    
    // Process task...
}
```

### 6. Instrument Token Generation

Add spans and metrics around token generation:

```cpp
void generate_tokens(server_slot& slot) {
    LLAMA_OTEL_SPAN("generate_tokens");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Generate token...
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    LLAMA_OTEL_INCREMENT_TOKENS(1);
    LLAMA_OTEL_RECORD_TOKEN_TIME(duration_ms);
    LLAMA_OTEL_ADD_ATTRIBUTE("token.id", std::to_string(token_id));
}
```

### 7. Instrument Model Loading

Add spans and metrics around model loading:

```cpp
void load_model() {
    LLAMA_OTEL_SPAN("load_model");
    LLAMA_OTEL_ADD_ATTRIBUTE("model.path", model_path);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Load model...
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    LLAMA_OTEL_RECORD_MODEL_LOAD_TIME(duration_ms);
    LLAMA_OTEL_ADD_ATTRIBUTE("model.loaded", "true");
}
```

### 8. Track Memory Usage

Add periodic memory usage tracking:

```cpp
void update_memory_metrics(llama_context* ctx) {
    if (ctx) {
        LLAMA_OTEL_RECORD_MEMORY_USAGE(llama_get_kv_cache_token_count(ctx) * llama_n_embd(llama_get_model(ctx)) * sizeof(float));
    }
}
```

### 9. Track Active Requests

Add metrics for tracking active requests:

```cpp
void update_active_requests_metrics() {
    LLAMA_OTEL_SET_ACTIVE_REQUESTS(active_request_count);
}
```

## Implementation Files

1. **tools/server/server.cpp**: Main server file to instrument
2. **common/otel/otel.h**: OTEL wrapper header
3. **common/otel/otel.cpp**: OTEL wrapper implementation

## Expected Benefits

1. **Performance Insights**: Identify bottlenecks in token generation and request handling
2. **Error Tracking**: Quickly identify and diagnose error conditions
3. **Resource Utilization**: Monitor memory usage patterns and optimize accordingly
4. **User Experience Metrics**: Track response times and throughput
5. **System Health**: Monitor overall system health and performance