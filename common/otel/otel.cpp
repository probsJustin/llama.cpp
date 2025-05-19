#include "otel.h"
#include "../log.h"

#if LLAMA_OTEL_ENABLED
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#endif

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace llama {
namespace otel {

#if LLAMA_OTEL_ENABLED
// Global storage for telemetry objects
namespace {
    std::shared_ptr<opentelemetry::trace::TracerProvider> g_tracer_provider;
    std::shared_ptr<opentelemetry::metrics::MeterProvider> g_meter_provider;
    std::shared_ptr<opentelemetry::metrics::Counter<int64_t>> g_tokens_counter;
    std::shared_ptr<opentelemetry::metrics::Histogram<double>> g_token_time_histogram;
    std::shared_ptr<opentelemetry::metrics::Histogram<double>> g_model_load_time_histogram;
    std::shared_ptr<opentelemetry::metrics::UpDownCounter<int64_t>> g_memory_usage_counter;
    std::shared_ptr<opentelemetry::metrics::Histogram<int64_t>> g_batch_size_histogram;
    std::shared_ptr<opentelemetry::metrics::UpDownCounter<int64_t>> g_active_requests_counter;
}
#endif

ScopedSpan::ScopedSpan(const std::string& name,
                     const std::unordered_map<std::string, std::string>& attributes) {
#if LLAMA_OTEL_ENABLED
    auto tracer = GetTracer();
    if (tracer) {
        auto span = tracer->StartSpan(name);
        for (const auto& attr : attributes) {
            span->SetAttribute(attr.first, attr.second);
        }
        span_ = std::move(span);
        active_ = true;
    }
#endif
}

ScopedSpan::ScopedSpan(ScopedSpan& parent,
                     const std::string& name,
                     const std::unordered_map<std::string, std::string>& attributes) {
#if LLAMA_OTEL_ENABLED
    if (parent.active_) {
        auto tracer = GetTracer();
        if (tracer) {
            auto span = tracer->StartSpan(name);
            for (const auto& attr : attributes) {
                span->SetAttribute(attr.first, attr.second);
            }
            span_ = std::move(span);
            active_ = true;
        }
    }
#endif
}

ScopedSpan::~ScopedSpan() {
#if LLAMA_OTEL_ENABLED
    if (active_ && span_) {
        span_->End();
        active_ = false;
    }
#endif
}

void ScopedSpan::AddAttribute(const std::string& key, const std::string& value) {
#if LLAMA_OTEL_ENABLED
    if (active_ && span_) {
        span_->SetAttribute(key, value);
    }
#endif
}

void ScopedSpan::AddEvent(const std::string& name,
                        const std::unordered_map<std::string, std::string>& attributes) {
#if LLAMA_OTEL_ENABLED
    if (active_ && span_) {
        span_->AddEvent(name);
    }
#endif
}

void ScopedSpan::RecordException(const std::exception& exception) {
#if LLAMA_OTEL_ENABLED
    if (active_ && span_) {
        span_->SetStatus(opentelemetry::trace::StatusCode::kError, exception.what());
    }
#endif
}

void ScopedSpan::SetError(const std::string& message) {
#if LLAMA_OTEL_ENABLED
    if (active_ && span_) {
        span_->SetStatus(opentelemetry::trace::StatusCode::kError, message);
    }
#endif
}

bool Initialize(const std::string& service_name,
               const std::string& service_version,
               const std::string& collector_endpoint) {
#if LLAMA_OTEL_ENABLED
    try {
        // Create resource for service identification
        auto resource = opentelemetry::sdk::resource::Resource::Create({
            {"service.name", service_name},
            {"service.version", service_version}
        });

        // Create OTLP exporter
        auto exporter = opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(
            {{"endpoint", collector_endpoint}});

        // Create batch processor
        auto processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(std::move(exporter));

        // Create tracer provider
        g_tracer_provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(
            std::move(processor), resource);

        // Set global tracer provider
        opentelemetry::trace::Provider::SetTracerProvider(g_tracer_provider);

        // Create OTLP metrics exporter
        auto metrics_exporter = opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
            {{"endpoint", collector_endpoint}});

        // Create metrics reader with exporter
        auto metrics_reader = std::make_unique<opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
            std::move(metrics_exporter), opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{});

        // Create metrics view registry
        auto view_registry = std::make_shared<opentelemetry::sdk::metrics::ViewRegistry>();

        // Create meter provider
        g_meter_provider = std::make_shared<opentelemetry::sdk::metrics::MeterProvider>(
            std::move(view_registry), resource);

        // Add metrics reader to meter provider
        std::static_pointer_cast<opentelemetry::sdk::metrics::MeterProvider>(g_meter_provider)
            ->AddMetricReader(std::move(metrics_reader));

        // Set global meter provider
        opentelemetry::metrics::Provider::SetMeterProvider(g_meter_provider);

        // Get meter and create instruments
        auto meter = GetMeter();
        if (meter) {
            g_tokens_counter = meter->CreateCounter<int64_t>("llama.tokens.count", 
                "Number of tokens generated", "tokens");

            g_token_time_histogram = meter->CreateHistogram<double>("llama.token.time", 
                "Time to generate each token", "ms");

            g_model_load_time_histogram = meter->CreateHistogram<double>("llama.model.load.time", 
                "Time to load a model", "ms");

            g_memory_usage_counter = meter->CreateUpDownCounter<int64_t>("llama.memory.usage", 
                "Memory usage", "bytes");

            g_batch_size_histogram = meter->CreateHistogram<int64_t>("llama.batch.size", 
                "Token batch size", "tokens");

            g_active_requests_counter = meter->CreateUpDownCounter<int64_t>("llama.requests.active", 
                "Number of active requests", "requests");
        }

        LOG_INFO("OpenTelemetry initialized successfully with endpoint %s", collector_endpoint.c_str());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize OpenTelemetry: %s", e.what());
        return false;
    }
#else
    LOG_INFO("OpenTelemetry support is not enabled in this build");
    return false;
#endif
}

void Shutdown() {
#if LLAMA_OTEL_ENABLED
    if (g_tracer_provider) {
        std::static_pointer_cast<opentelemetry::sdk::trace::TracerProvider>(g_tracer_provider)->Shutdown();
        g_tracer_provider.reset();
    }

    if (g_meter_provider) {
        std::static_pointer_cast<opentelemetry::sdk::metrics::MeterProvider>(g_meter_provider)->Shutdown();
        g_meter_provider.reset();
    }

    g_tokens_counter.reset();
    g_token_time_histogram.reset();
    g_model_load_time_histogram.reset();
    g_memory_usage_counter.reset();
    g_batch_size_histogram.reset();
    g_active_requests_counter.reset();
#endif
}

#if LLAMA_OTEL_ENABLED
std::shared_ptr<opentelemetry::trace::Tracer> GetTracer(const std::string& name) {
    static std::mutex mutex;
    static std::unordered_map<std::string, std::shared_ptr<opentelemetry::trace::Tracer>> tracers;

    std::lock_guard<std::mutex> lock(mutex);
    auto it = tracers.find(name);
    if (it == tracers.end()) {
        auto provider = opentelemetry::trace::Provider::GetTracerProvider();
        if (provider) {
            auto tracer = provider->GetTracer(name);
            tracers[name] = tracer;
            return tracer;
        }
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<opentelemetry::metrics::Meter> GetMeter(const std::string& name) {
    static std::mutex mutex;
    static std::unordered_map<std::string, std::shared_ptr<opentelemetry::metrics::Meter>> meters;

    std::lock_guard<std::mutex> lock(mutex);
    auto it = meters.find(name);
    if (it == meters.end()) {
        auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
        if (provider) {
            auto meter = provider->GetMeter(name);
            meters[name] = meter;
            return meter;
        }
        return nullptr;
    }
    return it->second;
}
#endif

void IncrementTokens(int64_t count) {
#if LLAMA_OTEL_ENABLED
    if (g_tokens_counter) {
        g_tokens_counter->Add(count);
    }
#endif
}

void RecordTokenTime(double milliseconds) {
#if LLAMA_OTEL_ENABLED
    if (g_token_time_histogram) {
        g_token_time_histogram->Record(milliseconds);
    }
#endif
}

void RecordModelLoadTime(double milliseconds) {
#if LLAMA_OTEL_ENABLED
    if (g_model_load_time_histogram) {
        g_model_load_time_histogram->Record(milliseconds);
    }
#endif
}

void RecordMemoryUsage(int64_t bytes) {
#if LLAMA_OTEL_ENABLED
    if (g_memory_usage_counter) {
        g_memory_usage_counter->Add(bytes);
    }
#endif
}

void RecordBatchSize(int64_t size) {
#if LLAMA_OTEL_ENABLED
    if (g_batch_size_histogram) {
        g_batch_size_histogram->Record(size);
    }
#endif
}

void SetActiveRequests(int64_t count) {
#if LLAMA_OTEL_ENABLED
    static std::mutex mutex;
    static int64_t last_count = 0;

    std::lock_guard<std::mutex> lock(mutex);
    if (g_active_requests_counter) {
        int64_t diff = count - last_count;
        if (diff != 0) {
            g_active_requests_counter->Add(diff);
            last_count = count;
        }
    }
#endif
}

} // namespace otel
} // namespace llama