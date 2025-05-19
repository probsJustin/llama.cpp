# Core llama.cpp Instrumentation Plan for OpenTelemetry

## Key Areas for Instrumentation

1. **Model Loading**
   - Track model loading time
   - Monitor memory allocation during loading
   - Count tensor loading operations
   - Measure KV cache initialization time

2. **Inference Operations**
   - Measure `llama_decode` execution time
   - Track token-by-token generation time
   - Monitor batch processing performance

3. **Memory Management**
   - Track KV cache usage
   - Monitor memory allocations and deallocations
   - Measure context window utilization

4. **Backend Operations**
   - Track GPU/CPU backend initialization time
   - Measure tensor operations performance
   - Monitor device utilization

5. **Error Handling**
   - Track error occurrences
   - Monitor cancellations and timeouts

## Implementation Approach

### 1. Add OpenTelemetry Headers

```cpp
#include "common/otel/otel.h"
```

### 2. Instrument Model Loading

Add spans and metrics around model loading:

```cpp
static int llama_model_load(const std::string & fname, std::vector<std::string> & splits, llama_model & model, llama_model_params & params) {
    // existing code
    model.t_load_us = 0;
    time_meas tm(model.t_load_us);
    
    model.t_start_us = tm.t_start_us;
    
    // Add OTEL instrumentation
    LLAMA_OTEL_SPAN("llama_model_load");
    LLAMA_OTEL_ADD_ATTRIBUTE("model.path", fname);
    LLAMA_OTEL_ADD_ATTRIBUTE("model.use_mmap", params.use_mmap ? "true" : "false");
    
    try {
        // Existing model loading code...
        
        // Record model loading metrics
        LLAMA_OTEL_RECORD_MODEL_LOAD_TIME(model.t_load_us / 1000.0); // convert us to ms
        LLAMA_OTEL_ADD_ATTRIBUTE("model.n_params", std::to_string(llama_info_params_count(model.hparams)));
        LLAMA_OTEL_ADD_ATTRIBUTE("model.n_tokens", std::to_string(model.hparams.n_vocab));
        
        return 0;
    } catch (const std::exception & err) {
        LLAMA_OTEL_SET_ERROR(err.what());
        // Existing error handling...
        return -1;
    }
}
```

### 3. Instrument Token Decoding

Add spans and metrics for token generation:

```cpp
int llama_decode(llama_context * ctx, llama_batch & batch) {
    LLAMA_OTEL_SPAN("llama_decode");
    LLAMA_OTEL_ADD_ATTRIBUTE("batch.n_tokens", std::to_string(batch.n_tokens));
    
    auto start_time = llama_time_us();
    
    // Existing decoding code...
    
    auto end_time = llama_time_us();
    auto duration_ms = (end_time - start_time) / 1000.0;
    
    LLAMA_OTEL_RECORD_TOKEN_TIME(duration_ms);
    LLAMA_OTEL_RECORD_BATCH_SIZE(batch.n_tokens);
    
    return result;
}
```

### 4. Instrument Context Creation and Management

Add spans for context creation:

```cpp
struct llama_context * llama_new_context_with_model(struct llama_model * model, const struct llama_context_params & params) {
    LLAMA_OTEL_SPAN("llama_new_context_with_model");
    
    // Existing context creation code...
    
    LLAMA_OTEL_RECORD_MEMORY_USAGE(ctx->mem_per_token * ctx->n_ctx);
    LLAMA_OTEL_ADD_ATTRIBUTE("context.n_ctx", std::to_string(ctx->n_ctx));
    
    return ctx;
}
```

### 5. Instrument Sampling Functions

Add spans for token sampling:

```cpp
llama_token llama_sample_token(struct llama_context * ctx, const struct llama_sampling_params & params) {
    LLAMA_OTEL_SPAN("llama_sample_token");
    LLAMA_OTEL_ADD_ATTRIBUTE("sampling.temp", std::to_string(params.temp));
    LLAMA_OTEL_ADD_ATTRIBUTE("sampling.top_p", std::to_string(params.top_p));
    
    // Existing sampling code...
    
    LLAMA_OTEL_INCREMENT_TOKENS(1);
    
    return result;
}
```

### 6. Instrument Backend Initialization

Add spans for backend initialization:

```cpp
void llama_backend_init(void) {
    LLAMA_OTEL_SPAN("llama_backend_init");
    
    // Existing initialization code...
    
    ggml_time_init();
    
    // needed to initialize f16 tables
    {
        struct ggml_init_params params = { 0, NULL, false };
        struct ggml_context * ctx = ggml_init(params);
        ggml_free(ctx);
    }
}
```

### 7. Instrument Memory Management

Add memory tracking:

```cpp
void llama_free(struct llama_context * ctx) {
    LLAMA_OTEL_SPAN("llama_free");
    
    if (ctx) {
        LLAMA_OTEL_ADD_ATTRIBUTE("context.tokens_evaluated", std::to_string(ctx->tokens_evaluated));
        LLAMA_OTEL_ADD_ATTRIBUTE("context.t_eval_us", std::to_string(ctx->t_eval_us));
        
        // Existing cleanup code...
    }
}
```

### 8. Instrument Error Handling

Add spans for error tracking:

```cpp
// In various error handling sections
if (error_condition) {
    LLAMA_OTEL_SET_ERROR("Error description");
    // Existing error handling...
}
```

## Implementation Files

1. **src/llama.cpp**: Main implementation file
2. **src/llama-impl.h**: Internal implementation header
3. **src/llama-context.cpp**: Context management
4. **src/llama-model.cpp**: Model management

## Expected Benefits

1. **Performance Profiling**: Identify bottlenecks in token generation and model loading
2. **Memory Optimization**: Track memory usage patterns for optimization
3. **Error Detection**: Quickly identify error conditions and patterns
4. **Resource Utilization**: Monitor CPU/GPU usage
5. **Latency Analysis**: Analyze end-to-end latency for various model sizes and operations