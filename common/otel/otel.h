#ifndef LLAMA_OTEL_H
#define LLAMA_OTEL_H

#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>

// Macro to enable/disable OpenTelemetry based on build configuration
#ifdef LLAMA_OTEL_ENABLE
#define LLAMA_OTEL_ENABLED 1
#else
#define LLAMA_OTEL_ENABLED 0
#endif

namespace llama {
namespace otel {

// Forward declarations for OpenTelemetry classes
namespace opentelemetry {
    namespace trace {
        class Span;
        class Tracer;
        class TracerProvider;
    }
    namespace metrics {
        class Meter;
        class MeterProvider;
    }
}

/**
 * @brief RAII wrapper for OpenTelemetry Span
 * 
 * This class creates a span on construction and ends it on destruction,
 * making it easy to properly scope spans in C++ code.
 */
class ScopedSpan {
public:
    /**
     * @brief Construct a new Scoped Span object
     * 
     * @param name Name of the span
     * @param attributes Optional attributes to add to the span
     */
    ScopedSpan(const std::string& name, 
               const std::unordered_map<std::string, std::string>& attributes = {});

    /**
     * @brief Construct a new Scoped Span as a child of another span
     * 
     * @param parent Parent span
     * @param name Name of the span
     * @param attributes Optional attributes to add to the span
     */
    ScopedSpan(ScopedSpan& parent, 
               const std::string& name,
               const std::unordered_map<std::string, std::string>& attributes = {});

    /**
     * @brief Destroy the Scoped Span object and end the span
     */
    ~ScopedSpan();

    /**
     * @brief Add an attribute to the span
     * 
     * @param key Attribute key
     * @param value Attribute value
     */
    void AddAttribute(const std::string& key, const std::string& value);

    /**
     * @brief Add an event to the span
     * 
     * @param name Event name
     * @param attributes Optional event attributes
     */
    void AddEvent(const std::string& name, 
                 const std::unordered_map<std::string, std::string>& attributes = {});

    /**
     * @brief Record an exception in the span
     * 
     * @param exception The exception to record
     */
    void RecordException(const std::exception& exception);

    /**
     * @brief Set span status to error with a message
     * 
     * @param message Error message
     */
    void SetError(const std::string& message);

private:
#if LLAMA_OTEL_ENABLED
    std::shared_ptr<opentelemetry::trace::Span> span_;
#endif
    bool active_ = false;
};

/**
 * @brief Initialize OpenTelemetry for llama.cpp
 * 
 * This function initializes the OpenTelemetry SDK and configures it
 * to export telemetry data to the specified collector endpoint.
 * 
 * @param service_name Name of the service (e.g., "llama-cpp-server")
 * @param service_version Version of the service
 * @param collector_endpoint OTLP endpoint (e.g., "http://localhost:4317")
 * @return true if initialization was successful, false otherwise
 */
bool Initialize(const std::string& service_name,
                const std::string& service_version,
                const std::string& collector_endpoint);

/**
 * @brief Shutdown OpenTelemetry and flush any pending telemetry data
 */
void Shutdown();

/**
 * @brief Get the global tracer
 * 
 * @param name Name of the tracer (optional)
 * @return Pointer to the tracer
 */
#if LLAMA_OTEL_ENABLED
std::shared_ptr<opentelemetry::trace::Tracer> GetTracer(const std::string& name = "llama-cpp");
#endif

/**
 * @brief Get the global meter
 * 
 * @param name Name of the meter (optional)
 * @return Pointer to the meter
 */
#if LLAMA_OTEL_ENABLED
std::shared_ptr<opentelemetry::metrics::Meter> GetMeter(const std::string& name = "llama-cpp");
#endif

// Counter for tracking token generation
void IncrementTokens(int64_t count = 1);

// Record token generation time in milliseconds
void RecordTokenTime(double milliseconds);

// Record model load time in milliseconds
void RecordModelLoadTime(double milliseconds);

// Record memory usage in bytes
void RecordMemoryUsage(int64_t bytes);

// Record batch size
void RecordBatchSize(int64_t size);

// Record active requests
void SetActiveRequests(int64_t count);

} // namespace otel
} // namespace llama

// Convenience macros for instrumentation

#if LLAMA_OTEL_ENABLED
#define LLAMA_OTEL_SPAN(name) llama::otel::ScopedSpan span(name)
#define LLAMA_OTEL_SPAN_WITH_PARENT(parent, name) llama::otel::ScopedSpan span(parent, name)
#define LLAMA_OTEL_ADD_ATTRIBUTE(key, value) span.AddAttribute(key, value)
#define LLAMA_OTEL_ADD_EVENT(name) span.AddEvent(name)
#define LLAMA_OTEL_RECORD_EXCEPTION(exception) span.RecordException(exception)
#define LLAMA_OTEL_SET_ERROR(message) span.SetError(message)
#define LLAMA_OTEL_INCREMENT_TOKENS(count) llama::otel::IncrementTokens(count)
#define LLAMA_OTEL_RECORD_TOKEN_TIME(milliseconds) llama::otel::RecordTokenTime(milliseconds)
#define LLAMA_OTEL_RECORD_MODEL_LOAD_TIME(milliseconds) llama::otel::RecordModelLoadTime(milliseconds)
#define LLAMA_OTEL_RECORD_MEMORY_USAGE(bytes) llama::otel::RecordMemoryUsage(bytes)
#define LLAMA_OTEL_RECORD_BATCH_SIZE(size) llama::otel::RecordBatchSize(size)
#define LLAMA_OTEL_SET_ACTIVE_REQUESTS(count) llama::otel::SetActiveRequests(count)
#else
#define LLAMA_OTEL_SPAN(name)
#define LLAMA_OTEL_SPAN_WITH_PARENT(parent, name)
#define LLAMA_OTEL_ADD_ATTRIBUTE(key, value)
#define LLAMA_OTEL_ADD_EVENT(name)
#define LLAMA_OTEL_RECORD_EXCEPTION(exception)
#define LLAMA_OTEL_SET_ERROR(message)
#define LLAMA_OTEL_INCREMENT_TOKENS(count)
#define LLAMA_OTEL_RECORD_TOKEN_TIME(milliseconds)
#define LLAMA_OTEL_RECORD_MODEL_LOAD_TIME(milliseconds)
#define LLAMA_OTEL_RECORD_MEMORY_USAGE(bytes)
#define LLAMA_OTEL_RECORD_BATCH_SIZE(size)
#define LLAMA_OTEL_SET_ACTIVE_REQUESTS(count)
#endif

#endif // LLAMA_OTEL_H