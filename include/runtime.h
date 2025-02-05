/**
 * =====================================================================================
 * 
 *        RUNTIME.H - Core Header for JavaScript Runtime (JSC + libuv Implementation)
 * 
 * =====================================================================================
 * 
 * This file contains the core interface definitions for the JavaScript runtime built
 * with JavaScriptCore (JSC) and libuv. It serves as the central API hub connecting
 * the JavaScript engine, event loop, and system APIs.
 * 
 * Key Components:
 * - JavaScript Engine: Manages JS context creation/execution using JSC
 * - Event Loop: libuv-based async I/O and timer handling
 * - System APIs: Bridge between native functionality and JS environment
 * 
 * Dependencies:
 * - JavaScriptCore (WebKitGTK/macOS SDK)
 * - libuv v1.40+
 * 
 * Architecture:
 * ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
 * │  JS Engine  │ ◄──►│ Event Loop  │ ◄──►│ System APIs │
 * └─────────────┘     └─────────────┘     └─────────────┘
 *        ▲                    ▲
 *        └────── Interop ─────┘
 * 
 * Memory Management:
 * - Uses JSC's garbage collection with manual protection for persistent handles
 * - libuv handles cleanup via close callbacks
 * 
 * =====================================================================================
 */

#ifndef RUNTIME_H
#define RUNTIME_H

#include <JavaScriptCore/JavaScript.h>
#include <uv.h>


// ================== JavaScript Engine Interface ================== //

/**
 * Creates a new JavaScript execution context with exposed system APIs
 * @return Initialized JSC global context
 */
JSGlobalContextRef create_js_context();

/**
 * Executes JavaScript code in the given context
 * @param ctx  Active JSC context
 * @param script  Null-terminated JS code string
 */
void execute_js(JSGlobalContextRef ctx, const char* script);

// ================== Event Loop Interface ================== //

extern uv_loop_t* loop;

/**
 * Initializes libuv's default event loop
 */
void init_event_loop();

/**
 * Starts the event loop (blocks until all handles are closed)
 */
void run_event_loop();


// ================== Timer API ================== //

extern uint32_t next_timer_id;

/**
 * Schedules a JS function to execute after specified delay
 * @param ctx       JS context for callback execution
 * @param callback  JS function reference
 * @param timeout   Delay in milliseconds
 */
void set_timeout(JSContextRef ctx, JSObjectRef callback, uint64_t timeout);
void clear_timeout(JSContextRef ctx, uint32_t timer_id);
void set_interval(JSContextRef ctx, JSObjectRef callback, uint64_t interval);
void clear_interval(JSContextRef ctx, uint32_t timer_id);


// ================== System API Interface ================== //

/**
 * Exposes native functionality to JS global scope
 * @param ctx  JS context to enhance with system APIs
 */
void expose_system_apis(JSGlobalContextRef ctx);

#endif // RUNTIME_H