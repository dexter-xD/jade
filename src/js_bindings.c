/**
 * =====================================================================================
 * 
 *        JS_BINDINGS.C - Native Functionality Bindings for JavaScript
 * 
 * =====================================================================================
 * 
 * This file implements the bridge between native capabilities and the JavaScript
 * environment. It exposes the following APIs to JavaScript:
 * - Console API (log, warn, info, debug, error)
 * - Timer API (setTimeout, clearTimeout, setInterval, clearInterval)
 * - Process API (argv, exit)
 * - Runtime Info (name, version)
 * 
 * Architecture:
 * ┌─────────────┐       ┌─────────────┐
 * │ JS Global   │ ◄─────│ Console API │
 * └─────────────┘       └─────────────┘
 *         ▲
 *         │ setTimeout
 * ┌─────────────┐
 * │ Event Loop  │
 * └─────────────┘
 * 
 * Key Features:
 * - Thread-safe JS/C interop
 * - Proper error handling
 * - Memory-safe string conversions
 * 
 * Security Notes:
 * - No input validation (demo purposes)
 * - Production should sanitize all inputs
 * 
 * =====================================================================================
 */

#include <JavaScriptCore/JavaScript.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "runtime.h"
#include "version.h"

// External variables
extern int process_argc;
extern char** process_argv;

// ================== Helper Functions ================== //

/**
 * Generates a timestamp string in the format "[HH:MM:SS]"
 * @return  Timestamp string (must be freed by caller)
 */
static char* get_timestamp() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    char* timestamp = malloc(10);  // "[HH:MM:SS]" + null terminator
    strftime(timestamp, 10, "[%H:%M:%S]", tm_info);
    return timestamp;
}

/**
 * Converts multiple JS values to a concatenated string
 * @param ctx       JS context
 * @param argc      Number of arguments
 * @param args      Array of JS values
 * @param exception Exception object (if any)
 * @return          Concatenated string (must be freed by caller)
 */
static char* js_values_to_string(JSContextRef ctx, size_t argc, const JSValueRef args[], JSValueRef* exception) {
    if (argc == 0) return strdup("");  // Handle no arguments

    // Temporary array to store C strings
    char** c_strs = malloc(argc * sizeof(char*));
    size_t total_len = 0;

    // First pass: convert all JS values to C strings
    for (size_t i = 0; i < argc; i++) {
        JSStringRef js_str = JSValueToStringCopy(ctx, args[i], exception);
        if (*exception) {
            // Cleanup on error
            for (size_t j = 0; j < i; j++) free(c_strs[j]);
            free(c_strs);
            return NULL;
        }

        size_t max_len = JSStringGetMaximumUTF8CStringSize(js_str);
        c_strs[i] = malloc(max_len);
        JSStringGetUTF8CString(js_str, c_strs[i], max_len);
        JSStringRelease(js_str);

        total_len += strlen(c_strs[i]) + 1;  // +1 for space or null terminator
    }

    // Allocate final string buffer
    char* result = malloc(total_len);
    result[0] = '\0';

    // Second pass: concatenate all strings
    for (size_t i = 0; i < argc; i++) {
        strcat(result, c_strs[i]);
        if (i < argc - 1) strcat(result, " ");
        free(c_strs[i]);  // Free individual strings
    }

    free(c_strs);
    return result;
}

// ================== Console API ================== //

/**
 * Generic console output function
 * @param prefix    Prefix for the log (e.g., "LOG", "WARN")
 * @param color     ANSI color code (e.g., "\033[34m" for blue)
 * @param ctx       JS context
 * @param function  JS function reference
 * @param thisObject JS `this` object
 * @param argc      Number of arguments
 * @param args      Array of JS values
 * @param exception Exception object (if any)
 */
static JSValueRef console_output(const char* prefix, const char* color, JSContextRef ctx, JSObjectRef function,
                                 JSObjectRef thisObject, size_t argc, const JSValueRef args[], JSValueRef* exception) {
    if (argc == 0) {
        printf("%s%s: \033[0m\n", color, prefix);  // Handle no arguments
        return JSValueMakeUndefined(ctx);
    }

    // Convert all arguments to a single string
    char* message = js_values_to_string(ctx, argc, args, exception);
    printf("%s%s: %s\033[0m\n", color, prefix, message);  // Print with color and prefix
    free(message);

    return JSValueMakeUndefined(ctx);
}

// Console methods
static JSValueRef console_log(JSContextRef ctx, JSObjectRef function, 
                              JSObjectRef thisObject, size_t argc, 
                              const JSValueRef args[], JSValueRef* exception) {
    return console_output("LOG", "\033[0m", ctx, function, thisObject, argc, args, exception);
}

static JSValueRef console_warn(JSContextRef ctx, JSObjectRef function, 
                               JSObjectRef thisObject, size_t argc, 
                               const JSValueRef args[], JSValueRef* exception) {
    return console_output("WARN", "\033[33m", ctx, function, thisObject, argc, args, exception);
}

static JSValueRef console_info(JSContextRef ctx, JSObjectRef function, 
                               JSObjectRef thisObject, size_t argc, 
                               const JSValueRef args[], JSValueRef* exception) {
    return console_output("INFO", "\033[34m", ctx, function, thisObject, argc, args, exception);
}

static JSValueRef console_debug(JSContextRef ctx, JSObjectRef function, 
                                JSObjectRef thisObject, size_t argc, 
                                const JSValueRef args[], JSValueRef* exception) {
    return console_output("DEBUG", "\033[90m", ctx, function, thisObject, argc, args, exception);
}

static JSValueRef console_error(JSContextRef ctx, JSObjectRef function,
                               JSObjectRef thisObject, size_t argc,
                               const JSValueRef args[], JSValueRef* exception) {
    return console_output("ERROR", "\033[31m", ctx, function, thisObject, argc, args, exception);
}

// ================== Timer API ================== //

/**
 * JS-accessible setTimeout implementation
 * @param args[0]  Callback function
 * @param args[1]  Delay in milliseconds
 */
static JSValueRef js_set_timeout(JSContextRef ctx, JSObjectRef function,
                                JSObjectRef thisObject, size_t argc,
                                const JSValueRef args[], JSValueRef* exception) {
    // Argument validation
    if (argc < 2) {
        JSStringRef msg = JSStringCreateWithUTF8CString("setTimeout requires 2 arguments");
        *exception = JSValueMakeString(ctx, msg);
        JSStringRelease(msg);
        return JSValueMakeUndefined(ctx);
    }

    // Extract callback and delay
    JSObjectRef callback = JSValueToObject(ctx, args[0], exception);
    uint64_t delay = JSValueToNumber(ctx, args[1], exception);
    
    // Schedule with event loop
    set_timeout(ctx, callback, delay);
    return JSValueMakeUndefined(ctx);
}

/**
 * clearTimeout - JS-accessible function
 */
static JSValueRef js_clear_timeout(JSContextRef ctx, JSObjectRef function,
                                   JSObjectRef thisObject, size_t argc,
                                   const JSValueRef args[], JSValueRef* exception) {
    if (argc < 1) {
        JSStringRef msg = JSStringCreateWithUTF8CString("clearTimeout requires 1 argument");
        *exception = JSValueMakeString(ctx, msg);
        JSStringRelease(msg);
        return JSValueMakeUndefined(ctx);
    }

    uint32_t timer_id = (uint32_t)JSValueToNumber(ctx, args[0], exception);
    if (*exception) return JSValueMakeUndefined(ctx);

    clear_timeout(ctx, timer_id);
    return JSValueMakeUndefined(ctx);
}

/**
 * setInterval - JS-accessible function
 */
static JSValueRef js_set_interval(JSContextRef ctx, JSObjectRef function,
                                  JSObjectRef thisObject, size_t argc,
                                  const JSValueRef args[], JSValueRef* exception) {
    if (argc < 2) {
        JSStringRef msg = JSStringCreateWithUTF8CString("setInterval requires 2 arguments");
        *exception = JSValueMakeString(ctx, msg);
        JSStringRelease(msg);
        return JSValueMakeUndefined(ctx);
    }

    JSObjectRef callback = JSValueToObject(ctx, args[0], exception);
    uint64_t interval = (uint64_t)JSValueToNumber(ctx, args[1], exception);
    if (*exception) return JSValueMakeUndefined(ctx);

    set_interval(ctx, callback, interval);
    return JSValueMakeNumber(ctx, next_timer_id - 1);  // Return timer ID
}

/**
 * clearInterval - JS-accessible function
 */
static JSValueRef js_clear_interval(JSContextRef ctx, JSObjectRef function,
                                    JSObjectRef thisObject, size_t argc,
                                    const JSValueRef args[], JSValueRef* exception) {
    if (argc < 1) {
        JSStringRef msg = JSStringCreateWithUTF8CString("clearInterval requires 1 argument");
        *exception = JSValueMakeString(ctx, msg);
        JSStringRelease(msg);
        return JSValueMakeUndefined(ctx);
    }

    uint32_t timer_id = (uint32_t)JSValueToNumber(ctx, args[0], exception);
    if (*exception) return JSValueMakeUndefined(ctx);

    clear_interval(ctx, timer_id);
    return JSValueMakeUndefined(ctx);
}

// ================== Process API ================== //

/**
 * process.exit - JS-accessible exit function
 */
static JSValueRef js_process_exit(JSContextRef ctx, JSObjectRef function,
                                 JSObjectRef thisObject, size_t argc,
                                 const JSValueRef args[], JSValueRef* exception) {
    int code = 0;
    if (argc > 0) {
        code = (int)JSValueToNumber(ctx, args[0], exception);
        if (*exception) return JSValueMakeUndefined(ctx);
    }

    uv_stop(loop);
    exit(code);
    return JSValueMakeUndefined(ctx);
}

// ================== API Exposure ================== //

/**
 * Exposes native APIs to JS global scope
 * @param ctx  Context to enhance
 */
void bind_js_native_apis(JSGlobalContextRef ctx) {
    // Create global object
    JSObjectRef global = JSContextGetGlobalObject(ctx);

    // ================== Console API ================== //
    JSObjectRef console = JSObjectMake(ctx, NULL, NULL);
    JSStringRef consoleName = JSStringCreateWithUTF8CString("console");
    JSObjectSetProperty(ctx, global, consoleName, console, kJSPropertyAttributeNone, NULL);
    JSStringRelease(consoleName);

    // Add console methods
    JSStringRef logName = JSStringCreateWithUTF8CString("log");
    JSObjectSetProperty(ctx, console, logName, JSObjectMakeFunctionWithCallback(ctx, logName, console_log), kJSPropertyAttributeNone, NULL);
    JSStringRelease(logName);

    JSStringRef warnName = JSStringCreateWithUTF8CString("warn");
    JSObjectSetProperty(ctx, console, warnName, JSObjectMakeFunctionWithCallback(ctx, warnName, console_warn), kJSPropertyAttributeNone, NULL);
    JSStringRelease(warnName);

    JSStringRef infoName = JSStringCreateWithUTF8CString("info");
    JSObjectSetProperty(ctx, console, infoName, JSObjectMakeFunctionWithCallback(ctx, infoName, console_info), kJSPropertyAttributeNone, NULL);
    JSStringRelease(infoName);

    JSStringRef debugName = JSStringCreateWithUTF8CString("debug");
    JSObjectSetProperty(ctx, console, debugName, JSObjectMakeFunctionWithCallback(ctx, debugName, console_debug), kJSPropertyAttributeNone, NULL);
    JSStringRelease(debugName);

    JSStringRef errorName = JSStringCreateWithUTF8CString("error");
    JSObjectSetProperty(ctx, console, errorName, JSObjectMakeFunctionWithCallback(ctx, errorName, console_error), kJSPropertyAttributeNone, NULL);
    JSStringRelease(errorName);

    // ================== Timer API ================== //
    JSStringRef setTimeoutName = JSStringCreateWithUTF8CString("setTimeout");
    JSObjectSetProperty(ctx, global, setTimeoutName, JSObjectMakeFunctionWithCallback(ctx, setTimeoutName, js_set_timeout), kJSPropertyAttributeNone, NULL);
    JSStringRelease(setTimeoutName);

    JSStringRef clearTimeoutName = JSStringCreateWithUTF8CString("clearTimeout");
    JSObjectSetProperty(ctx, global, clearTimeoutName, JSObjectMakeFunctionWithCallback(ctx, clearTimeoutName, js_clear_timeout), kJSPropertyAttributeNone, NULL);
    JSStringRelease(clearTimeoutName);

    JSStringRef setIntervalName = JSStringCreateWithUTF8CString("setInterval");
    JSObjectSetProperty(ctx, global, setIntervalName, JSObjectMakeFunctionWithCallback(ctx, setIntervalName, js_set_interval), kJSPropertyAttributeNone, NULL);
    JSStringRelease(setIntervalName);

    JSStringRef clearIntervalName = JSStringCreateWithUTF8CString("clearInterval");
    JSObjectSetProperty(ctx, global, clearIntervalName, JSObjectMakeFunctionWithCallback(ctx, clearIntervalName, js_clear_interval), kJSPropertyAttributeNone, NULL);
    JSStringRelease(clearIntervalName);

    // ================== Process API ================== //
    JSObjectRef process = JSObjectMake(ctx, NULL, NULL);
    JSStringRef processName = JSStringCreateWithUTF8CString("process");
    JSObjectSetProperty(ctx, global, processName, process, kJSPropertyAttributeNone, NULL);
    JSStringRelease(processName);

    // Add process.argv
    JSObjectRef argv = JSObjectMakeArray(ctx, 0, NULL, NULL);
    for (int i = 0; i < process_argc; i++) {
        JSStringRef arg = JSStringCreateWithUTF8CString(process_argv[i]);
        JSObjectSetPropertyAtIndex(ctx, argv, i, JSValueMakeString(ctx, arg), NULL);
        JSStringRelease(arg);
    }
    JSStringRef argvName = JSStringCreateWithUTF8CString("argv");
    JSObjectSetProperty(ctx, process, argvName, argv, kJSPropertyAttributeNone, NULL);
    JSStringRelease(argvName);

    // Add process.exit
    JSStringRef exitName = JSStringCreateWithUTF8CString("exit");
    JSObjectSetProperty(ctx, process, exitName, JSObjectMakeFunctionWithCallback(ctx, exitName, js_process_exit), kJSPropertyAttributeNone, NULL);
    JSStringRelease(exitName);

    // ================== HTTP API ================== //
    JSObjectRef http = JSObjectMake(ctx, NULL, NULL);
    JSStringRef httpName = JSStringCreateWithUTF8CString("http");
    JSObjectSetProperty(ctx, global, httpName, http, kJSPropertyAttributeNone, NULL);
    JSStringRelease(httpName);

    // Add http.get
    JSStringRef getName = JSStringCreateWithUTF8CString("get");
    JSObjectSetProperty(ctx, http, getName, JSObjectMakeFunctionWithCallback(ctx, getName, http_get), kJSPropertyAttributeNone, NULL);
    JSStringRelease(getName);

    // Add http.post
    JSStringRef postName = JSStringCreateWithUTF8CString("post");
    JSObjectSetProperty(ctx, http, postName, JSObjectMakeFunctionWithCallback(ctx, postName, http_post), kJSPropertyAttributeNone, NULL);
    JSStringRelease(postName);

    // Add http.put
    JSStringRef putName = JSStringCreateWithUTF8CString("put");
    JSObjectSetProperty(ctx, http, putName, JSObjectMakeFunctionWithCallback(ctx, putName, http_put), kJSPropertyAttributeNone, NULL);
    JSStringRelease(putName);

    // Add http.delete
    JSStringRef deleteName = JSStringCreateWithUTF8CString("delete");
    JSObjectSetProperty(ctx, http, deleteName, JSObjectMakeFunctionWithCallback(ctx, deleteName, http_delete), kJSPropertyAttributeNone, NULL);
    JSStringRelease(deleteName);

    // Add http.createServer
    JSStringRef createServerName = JSStringCreateWithUTF8CString("createServer");
    JSObjectSetProperty(ctx, http, createServerName, JSObjectMakeFunctionWithCallback(ctx, createServerName, http_create_server), kJSPropertyAttributeNone, NULL);
    JSStringRelease(createServerName);
}