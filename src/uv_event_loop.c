/**
 * =====================================================================================
 * 
 *        UV_EVENT_LOOP.C - libuv-based Event Loop Implementation
 * 
 * =====================================================================================
 * 
 * Implements:
 * - Async I/O operations
 * - Timer scheduling
 * - Event loop lifecycle management
 * 
 * Architecture:
 * ┌───────────────┐       ┌───────────────┐
 * │  UV EventLoop │ ◄─────│ Timer Handles │
 * └───────────────┘       └───────────────┘
 *         ▲
 *         │ Handles async ops
 * ┌───────────────┐
 * │ JS Callbacks  │
 * └───────────────┘
 * 
 * Key Features:
 * - Single-threaded async concurrency
 * - Timer resolution up to 1ms
 * - Automatic handle cleanup
 * 
 * Memory Management:
 * - Manual allocation for timer requests
 * - UV handle auto-closure with cleanup callbacks
 * 
 * =====================================================================================
 */

#include <uv.h>
#include <stdlib.h>
#include "runtime.h"

// Shared event loop instance
uv_loop_t* loop;

// Timer request structure
typedef struct {
    uv_timer_t timer;      // libuv timer handle
    JSContextRef ctx;      // JS context for callback
    JSObjectRef callback;  // JS function to execute
} TimerRequest;

/**
 * libuv timer callback - executes JS timeout handler
 * @param handle  libuv timer handle with attached TimerRequest
 */
static void on_timeout(uv_timer_t* handle) {
    TimerRequest* tr = (TimerRequest*)handle->data;
    
    // Protect callback from GC during execution
    JSValueProtect(tr->ctx, tr->callback);
    
    // Execute JS callback with dummy argument
    JSValueRef args[] = { JSValueMakeNumber(tr->ctx, 0) };
    JSObjectCallAsFunction(tr->ctx, tr->callback, NULL, 1, args, NULL);
    
    // Cleanup resources
    JSValueUnprotect(tr->ctx, tr->callback);
    uv_timer_stop(&tr->timer);
    uv_close((uv_handle_t*)&tr->timer, NULL);
    free(tr);
}

/**
 * Schedules JS callback execution after delay
 * @param timeout  Delay in milliseconds
 */
void set_timeout(JSContextRef ctx, JSObjectRef callback, uint64_t timeout) {
    // Allocate timer request structure
    TimerRequest* tr = malloc(sizeof(TimerRequest));
    tr->ctx = ctx;
    tr->callback = callback;
    
    // Initialize libuv timer
    uv_timer_init(loop, &tr->timer);
    tr->timer.data = tr;
    
    // Start timer with single-shot configuration
    uv_timer_start(&tr->timer, on_timeout, timeout, 0);
    
    // Prevent GC from collecting callback during wait
    JSValueProtect(ctx, callback);
}

/**
 * Initializes libuv's default event loop
 */
void init_event_loop() {
    loop = uv_default_loop();
}

/**
 * Runs event loop until all active handles are closed
 */
void run_event_loop() {
    uv_run(loop, UV_RUN_DEFAULT);
}