#include <JavaScriptCore/JavaScript.h>
#include <uv.h>
#include <stdint.h>
#include <stdlib.h>
#include "runtime.h"

// Event Loop Definitions
uv_loop_t* loop = NULL;

void init_event_loop(void) {
    loop = uv_default_loop();
}

void run_event_loop(void) {
    uv_run(loop, UV_RUN_DEFAULT);
}

// Timer request structure
typedef struct {
    uv_timer_t timer;
    JSContextRef ctx;
    JSObjectRef callback;
    uint32_t id;
} TimerRequest;

// Timer registry
static TimerRequest* timer_registry[1024];  // Fixed-size registry for simplicity
uint32_t next_timer_id = 0;

// Timer callback handler
static void on_timer(uv_timer_t* handle) {
    TimerRequest* tr = (TimerRequest*)handle->data;
    JSValueProtect(tr->ctx, tr->callback);
    JSValueRef args[] = { JSValueMakeNumber(tr->ctx, 0) };
    JSObjectCallAsFunction(tr->ctx, tr->callback, NULL, 1, args, NULL);
    JSValueUnprotect(tr->ctx, tr->callback);
}

void set_timeout(JSContextRef ctx, JSObjectRef callback, uint64_t timeout) {
    TimerRequest* tr = malloc(sizeof(TimerRequest));
    tr->ctx = ctx;
    tr->callback = callback;
    tr->id = next_timer_id++;
    timer_registry[tr->id] = tr;  // Add to registry

    uv_timer_init(loop, &tr->timer);
    tr->timer.data = tr;
    uv_timer_start(&tr->timer, on_timer, timeout, 0);  // No repeat
    JSValueProtect(ctx, callback);
}

void set_interval(JSContextRef ctx, JSObjectRef callback, uint64_t interval) {
    TimerRequest* tr = malloc(sizeof(TimerRequest));
    tr->ctx = ctx;
    tr->callback = callback;
    tr->id = next_timer_id++;
    timer_registry[tr->id] = tr;  // Add to registry

    uv_timer_init(loop, &tr->timer);
    tr->timer.data = tr;
    uv_timer_start(&tr->timer, on_timer, interval, interval);  // Repeat
    JSValueProtect(ctx, callback);
}

void clear_timeout(JSContextRef ctx, uint32_t timer_id) {
    if (timer_id >= next_timer_id || !timer_registry[timer_id]) {
        return;  // Invalid timer ID
    }

    TimerRequest* tr = timer_registry[timer_id];
    uv_timer_stop(&tr->timer);
    uv_close((uv_handle_t*)&tr->timer, NULL);
    JSValueUnprotect(tr->ctx, tr->callback);
    timer_registry[timer_id] = NULL;  // Remove from registry
    free(tr);
}

void clear_interval(JSContextRef ctx, uint32_t timer_id) {
    clear_timeout(ctx, timer_id);  // Reuse clear_timeout logic
}