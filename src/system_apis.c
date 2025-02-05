/**
 * =====================================================================================
 * 
 *        SYSTEM_APIS.C - Native Functionality Exposure to JavaScript
 * 
 * =====================================================================================
 * 
 * Implements bridge between native capabilities and JS environment:
 * - Console API (log/error)
 * - Timer API (setTimeout)
 * - Future expansion: FS, Network, etc.
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
#include "runtime.h"

// ================== Helper Function ================== //

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


// ================== Console API Implementations ================== //

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

/**
 * console.log - Standard log output
 */
static JSValueRef console_log(JSContextRef ctx, JSObjectRef function, 
                              JSObjectRef thisObject, size_t argc, 
                              const JSValueRef args[], JSValueRef* exception) {
    return console_output("LOG", "\033[0m", ctx, function, thisObject, argc, args, exception);
}


/**
 * console.warn - Warning output (highlighted)
 */
static JSValueRef console_warn(JSContextRef ctx, JSObjectRef function, 
                               JSObjectRef thisObject, size_t argc, 
                               const JSValueRef args[], JSValueRef* exception) {
    return console_output("WARN", "\033[33m", ctx, function, thisObject, argc, args, exception);
}

/**
 * console.info - Informational output
 */
static JSValueRef console_info(JSContextRef ctx, JSObjectRef function, 
                               JSObjectRef thisObject, size_t argc, 
                               const JSValueRef args[], JSValueRef* exception) {
    return console_output("INFO", "\033[34m", ctx, function, thisObject, argc, args, exception);
}

/**
 * console.debug - Debug output (low priority)
 */
static JSValueRef console_debug(JSContextRef ctx, JSObjectRef function, 
                                JSObjectRef thisObject, size_t argc, 
                                const JSValueRef args[], JSValueRef* exception) {
    return console_output("DEBUG", "\033[90m", ctx, function, thisObject, argc, args, exception);
}

/**
 * console.error - Error output (highlighted in red)
 */
static JSValueRef console_error(JSContextRef ctx, JSObjectRef function,
                               JSObjectRef thisObject, size_t argc,
                               const JSValueRef args[], JSValueRef* exception) {
    return console_output("ERROR", "\033[31m", ctx, function, thisObject, argc, args, exception);
}


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
 * Exposes native APIs to JS global scope
 * @param ctx  Context to enhance
 */
void expose_system_apis(JSGlobalContextRef ctx) {
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    
    // Create console object
    JSObjectRef console = JSObjectMake(ctx, NULL, NULL);
    JSStringRef console_name = JSStringCreateWithUTF8CString("console");
    JSObjectSetProperty(ctx, global, console_name, console, kJSPropertyAttributeNone, NULL);
    JSStringRelease(console_name);
    
    // Add console.log
    JSStringRef log_name = JSStringCreateWithUTF8CString("log");
    JSObjectRef log_func = JSObjectMakeFunctionWithCallback(ctx, log_name, console_log);
    JSObjectSetProperty(ctx, console, log_name, log_func, kJSPropertyAttributeNone, NULL);
    JSStringRelease(log_name);
    
    // Add console.warn
    JSStringRef warn_name = JSStringCreateWithUTF8CString("warn");
    JSObjectRef warn_func = JSObjectMakeFunctionWithCallback(ctx, warn_name, console_warn);
    JSObjectSetProperty(ctx, console, warn_name, warn_func, kJSPropertyAttributeNone, NULL);
    JSStringRelease(warn_name);
    
    // Add console.info
    JSStringRef info_name = JSStringCreateWithUTF8CString("info");
    JSObjectRef info_func = JSObjectMakeFunctionWithCallback(ctx, info_name, console_info);
    JSObjectSetProperty(ctx, console, info_name, info_func, kJSPropertyAttributeNone, NULL);
    JSStringRelease(info_name);
    
    // Add console.debug
    JSStringRef debug_name = JSStringCreateWithUTF8CString("debug");
    JSObjectRef debug_func = JSObjectMakeFunctionWithCallback(ctx, debug_name, console_debug);
    JSObjectSetProperty(ctx, console, debug_name, debug_func, kJSPropertyAttributeNone, NULL);
    JSStringRelease(debug_name);
    
    // Add console.error
    JSStringRef error_name = JSStringCreateWithUTF8CString("error");
    JSObjectRef error_func = JSObjectMakeFunctionWithCallback(ctx, error_name, console_error);
    JSObjectSetProperty(ctx, console, error_name, error_func, kJSPropertyAttributeNone, NULL);
    JSStringRelease(error_name);
    
    // Add global setTimeout
    JSStringRef setTimeout_name = JSStringCreateWithUTF8CString("setTimeout");
    JSObjectRef setTimeout_func = JSObjectMakeFunctionWithCallback(ctx, setTimeout_name, js_set_timeout);
    JSObjectSetProperty(ctx, global, setTimeout_name, setTimeout_func, kJSPropertyAttributeNone, NULL);
    JSStringRelease(setTimeout_name);
}