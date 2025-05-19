```diff
--- a/src/llama.cpp
+++ b/src/llama.cpp
@@ -7,6 +7,9 @@
 #include "llama-model-saver.h"
 #include "llama-model.h"
 
+#if LLAMA_OTEL_ENABLE
+#include "common/otel/otel.h"
+#endif
 #include "ggml.h"
 #include "ggml-backend.h"
 
@@ -89,9 +91,16 @@ static int llama_model_load(const std::string & fname, std::vector<std::string>
     // we take page faults deferred by mmap() into consideration
     model.t_load_us = 0;
     time_meas tm(model.t_load_us);
-
     model.t_start_us = tm.t_start_us;
 
+#if LLAMA_OTEL_ENABLE
+    LLAMA_OTEL_SPAN("llama_model_load");
+    LLAMA_OTEL_ADD_ATTRIBUTE("model.path", fname);
+    LLAMA_OTEL_ADD_ATTRIBUTE("model.use_mmap", params.use_mmap ? "true" : "false");
+    LLAMA_OTEL_ADD_ATTRIBUTE("model.vocab_only", params.vocab_only ? "true" : "false");
+    LLAMA_OTEL_ADD_ATTRIBUTE("model.split_count", std::to_string(splits.size()));
+#endif
+
     try {
         llama_model_loader ml(fname, splits, params.use_mmap, params.check_tensors, params.kv_overrides, params.tensor_buft_overrides);
 
@@ -153,11 +162,20 @@ static int llama_model_load(const std::string & fname, std::vector<std::string>
         }
 
         model.init_tmp();
+        
+#if LLAMA_OTEL_ENABLE
+        LLAMA_OTEL_RECORD_MODEL_LOAD_TIME(model.t_load_us / 1000.0); // Convert us to ms
+        LLAMA_OTEL_ADD_ATTRIBUTE("model.n_params", std::to_string(model.n_params()));
+#endif
 
         return 0;
     }
     catch (const std::exception & err) {
         LOG_ERROR_LLAMA("error loading model: %s", err.what());
+        
+#if LLAMA_OTEL_ENABLE
+        LLAMA_OTEL_SET_ERROR(err.what());
+#endif
         return -1;
     }
 }
@@ -442,8 +460,16 @@ void llama_free(llama_context * ctx) {
 }
 
 int llama_decode(llama_context * ctx, llama_batch & batch) {
+#if LLAMA_OTEL_ENABLE
+    LLAMA_OTEL_SPAN("llama_decode");
+    LLAMA_OTEL_ADD_ATTRIBUTE("batch.n_tokens", std::to_string(batch.n_tokens));
+    auto start_time = llama_time_us();
+#endif
+
     if (!batch.n_tokens) {
         LOG_ERROR_LLAMA("llama_decode: empty batch");
+#if LLAMA_OTEL_ENABLE
+        LLAMA_OTEL_SET_ERROR("empty batch");
+#endif
         return 1;
     }
 
@@ -453,6 +479,14 @@ int llama_decode(llama_context * ctx, llama_batch & batch) {
             llama_context_setup_batch(ctx, batch);
         } else {
             LOG_ERROR_LLAMA("llama_decode: no batch setup because the context has state");
+#if LLAMA_OTEL_ENABLE
+            LLAMA_OTEL_SET_ERROR("no batch setup because the context has state");
+#endif
             return 1;
         }
     }
 
     const int ret = llama_decode_internal(*ctx, batch);
+
+#if LLAMA_OTEL_ENABLE
+    auto end_time = llama_time_us();
+    auto duration_ms = (end_time - start_time) / 1000.0;
+    
+    LLAMA_OTEL_RECORD_TOKEN_TIME(duration_ms);
+    LLAMA_OTEL_RECORD_BATCH_SIZE(batch.n_tokens);
+    LLAMA_OTEL_INCREMENT_TOKENS(batch.n_tokens);
+#endif
+
     return ret;
 }
```

Next, let's add instrumentation to the server.cpp:

```diff
--- a/tools/server/server.cpp
+++ b/tools/server/server.cpp
@@ -7,6 +7,10 @@
 #include "log.h"
 #include "sampling.h"
 #include "speculative.h"
+
+#if LLAMA_OTEL_ENABLE
+#include "common/otel/otel.h"
+#endif
 #include "mtmd.h"
 
 // Change JSON_ASSERT from assert() to GGML_ASSERT:
@@ -1234,6 +1238,11 @@ void server_queue_add_task(server_queue& q, server_task task) {
     task.id = q.next_task_id++;
     {
         std::lock_guard<std::mutex> lock(q.mutex);
+        
+#if LLAMA_OTEL_ENABLE
+        LLAMA_OTEL_SPAN("server_queue_add_task");
+        LLAMA_OTEL_ADD_ATTRIBUTE("task.id", std::to_string(task.id));
+#endif
         q.queue.push_back(task);
     }
     q.condition.notify_one();
@@ -1308,6 +1317,11 @@ bool server_fill_word_and_get_next_token(const server_context& server_ctx, serve
     // sample the next token
     const int n_vocab = llama_n_vocab(ctx);
     llama_token token = -1;
+    
+#if LLAMA_OTEL_ENABLE
+    LLAMA_OTEL_SPAN("server_sample_token");
+    auto start_time = llama_time_us();
+#endif
 
     // When using grammar, the tokens can exist in the KV cache. This notifies the binary interface
     // that the following sampling was done after updating the cache, which impacts what is in the
@@ -1344,6 +1358,14 @@ bool server_fill_word_and_get_next_token(const server_context& server_ctx, serve
             slot.n_decoded += 1;
         }
     }
+    
+#if LLAMA_OTEL_ENABLE
+    auto end_time = llama_time_us();
+    auto duration_ms = (end_time - start_time) / 1000.0;
+    
+    LLAMA_OTEL_RECORD_TOKEN_TIME(duration_ms);
+    LLAMA_OTEL_INCREMENT_TOKENS(1);
+#endif
 
     // if there is no EOS, the string is incomplete, return true
     return token != llama_token_eos(model);
@@ -1363,8 +1385,13 @@ bool server_process_slot(server_context& server_ctx, server_slot& slot) {
     const int64_t t_launch_start_ms = log_time_ms();
 
     server_slot_stats& stats = slot.stats;
-
+    
     llama_context * ctx = slot.ctx;
+
+#if LLAMA_OTEL_ENABLE
+    LLAMA_OTEL_SPAN("server_process_slot");
+    LLAMA_OTEL_ADD_ATTRIBUTE("slot.id", std::to_string(slot.id));
+#endif
     
     // process in a stateful way
     switch (slot.state) {
@@ -1398,6 +1425,11 @@ bool server_process_slot(server_context& server_ctx, server_slot& slot) {
             // Start inference
             if (llama_get_kv_cache_token_count(ctx) == 0) {
                 LOG_INFO_SLOTS("token count: %zu/%zu (%.1f%%)", stats.n_prompt_tokens[slot.id_worker].count, slot.max_context_tokens, 100.0f * stats.n_prompt_tokens[slot.id_worker].count / slot.max_context_tokens);
+                
+#if LLAMA_OTEL_ENABLE
+                LLAMA_OTEL_SPAN("server_process_prompt");
+                LLAMA_OTEL_ADD_ATTRIBUTE("prompt.tokens", std::to_string(stats.n_prompt_tokens[slot.id_worker].count));
+#endif
 
                 if (stats.n_prompt_tokens[slot.id_worker].count > 0 && stats.n_prompt_tokens[slot.id_worker].count <= slot.max_context_tokens) {
                     slot.state = SLOT_STATE_PROCESSING_PROMPT;
@@ -1518,6 +1550,11 @@ bool server_process_slot(server_context& server_ctx, server_slot& slot) {
                 if (slot.complete && slot.params.stream) {
                     slot_send_done_response(*slot);
                 }
+                
+#if LLAMA_OTEL_ENABLE
+                LLAMA_OTEL_ADD_ATTRIBUTE("completion.tokens", std::to_string(stats.n_predicted_tokens[slot.id_worker].count));
+                LLAMA_OTEL_ADD_ATTRIBUTE("completion.duration_ms", std::to_string(stats.t_completion_us[slot.id_worker] / 1000.0));
+#endif
 
                 return false;
             }
```