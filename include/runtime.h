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


// =====================================================================================
//                          JAVASCRIPT ENGINE INTERFACE
// =====================================================================================

/**
 * Creates a new JavaScript execution context with exposed system APIs.
 * @return Initialized JSC global context.
 */
JSGlobalContextRef create_js_context();

/**
 * Executes JavaScript code in the given context.
 * @param ctx     Active JSC context.
 * @param script  Null-terminated JS code string.
 */
void execute_js(JSGlobalContextRef ctx, const char* script);


// =====================================================================================
//                          EVENT LOOP INTERFACE
// =====================================================================================

/**
 * Global event loop instance.
 */
extern uv_loop_t* loop;

/**
 * Initializes libuv's default event loop.
 */
void init_event_loop();

/**
 * Starts the event loop (blocks until all handles are closed).
 */
void run_event_loop();


// =====================================================================================
//                          TIMER API
// =====================================================================================

/**
 * Global timer ID tracker.
 */
extern uint32_t next_timer_id;

/**
 * Schedules a JS function to execute after a specified delay.
 * @param ctx       JS context for callback execution.
 * @param callback  JS function reference.
 * @param timeout   Delay in milliseconds.
 */
void set_timeout(JSContextRef ctx, JSObjectRef callback, uint64_t timeout);

/**
 * Cancels a scheduled timeout function.
 * @param ctx       JS context for callback execution.
 * @param timer_id  ID of the timeout to clear.
 */
void clear_timeout(JSContextRef ctx, uint32_t timer_id);

/**
 * Schedules a JS function to execute repeatedly at a fixed interval.
 * @param ctx       JS context for callback execution.
 * @param callback  JS function reference.
 * @param interval  Interval time in milliseconds.
 */
void set_interval(JSContextRef ctx, JSObjectRef callback, uint64_t interval);

/**
 * Cancels a scheduled interval function.
 * @param ctx       JS context for callback execution.
 * @param timer_id  ID of the interval to clear.
 */
void clear_interval(JSContextRef ctx, uint32_t timer_id);

/**
 * Asynchronous readFile function
 */
JSValueRef fs_read_file(JSContextRef ctx, JSObjectRef function,
                        JSObjectRef thisObject, size_t argc,
                        const JSValueRef args[], JSValueRef* exception);

/**
 * Asynchronous writeFile function
 */
JSValueRef fs_write_file(JSContextRef ctx, JSObjectRef function,
                         JSObjectRef thisObject, size_t argc,
                         const JSValueRef args[], JSValueRef* exception);

/**
 * Asynchronous exists function
 */
JSValueRef fs_exists(JSContextRef ctx, JSObjectRef function,
                     JSObjectRef thisObject, size_t argc,
                     const JSValueRef args[], JSValueRef* exception);



/**
 * Creates a TCP server that listens for connections.
 */
JSValueRef net_create_server(JSContextRef ctx, JSObjectRef function,
                             JSObjectRef thisObject, size_t argc,
                             const JSValueRef args[], JSValueRef* exception);

/**
 * Binds and starts the TCP server on a specified port.
 */
JSValueRef net_server_listen(JSContextRef ctx, JSObjectRef function,
                             JSObjectRef thisObject, size_t argc,
                             const JSValueRef args[], JSValueRef* exception);


/**
 * Writes data to a connected client socket.
 */
JSValueRef client_write(JSContextRef ctx, JSObjectRef function,
                        JSObjectRef thisObject, size_t argc,
                        const JSValueRef args[], JSValueRef* exception);



// =====================================================================================
//                          SYSTEM API INTERFACE
// =====================================================================================

/**
 * Binds native system APIs to the JavaScript global scope.
 * @param ctx  JS context to enhance with system APIs.
 */
void bind_js_native_apis(JSGlobalContextRef ctx);

#endif // RUNTIME_H