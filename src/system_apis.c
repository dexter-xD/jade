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
#include "runtime.h"

/**
 * console.log implementation
 * @param args[0]  Message to log
 */
static JSValueRef console_log(JSContextRef ctx, JSObjectRef function, 
                             JSObjectRef thisObject, size_t argc, 
                             const JSValueRef args[], JSValueRef* exception) {
    // Convert JS string to C string
    JSStringRef message = JSValueToStringCopy(ctx, args[0], exception);
    size_t len = JSStringGetMaximumUTF8CStringSize(message);
    char* cmsg = malloc(len);
    JSStringGetUTF8CString(message, cmsg, len);
    
    // Print to stdout
    printf("LOG: %s\n", cmsg);
    
    // Cleanup
    free(cmsg);
    JSStringRelease(message);
    return JSValueMakeUndefined(ctx);
}

/**
 * console.warn - Warning output (highlighted)
 * @param args[0]  Warning message
 */
static JSValueRef console_warn(JSContextRef ctx, JSObjectRef function, 
                              JSObjectRef thisObject, size_t argc, 
                              const JSValueRef args[], JSValueRef* exception) {
    JSStringRef message = JSValueToStringCopy(ctx, args[0], exception);
    size_t len = JSStringGetMaximumUTF8CStringSize(message);
    char* cmsg = malloc(len);
    JSStringGetUTF8CString(message, cmsg, len);
    fprintf(stderr, "\033[33mWARN: %s\033[0m\n", cmsg);  // Yellow color for warnings
    free(cmsg);
    JSStringRelease(message);
    return JSValueMakeUndefined(ctx);
}

/**
 * console.info - Informational output
 * @param args[0]  Info message
 */
static JSValueRef console_info(JSContextRef ctx, JSObjectRef function, 
                              JSObjectRef thisObject, size_t argc, 
                              const JSValueRef args[], JSValueRef* exception) {
    JSStringRef message = JSValueToStringCopy(ctx, args[0], exception);
    size_t len = JSStringGetMaximumUTF8CStringSize(message);
    char* cmsg = malloc(len);
    JSStringGetUTF8CString(message, cmsg, len);
    printf("\033[34mINFO: %s\033[0m\n", cmsg);  // Blue color for info
    free(cmsg);
    JSStringRelease(message);
    return JSValueMakeUndefined(ctx);
}

/**
 * console.debug - Debug output (low priority)
 * @param args[0]  Debug message
 */
static JSValueRef console_debug(JSContextRef ctx, JSObjectRef function, 
                               JSObjectRef thisObject, size_t argc, 
                               const JSValueRef args[], JSValueRef* exception) {
    JSStringRef message = JSValueToStringCopy(ctx, args[0], exception);
    size_t len = JSStringGetMaximumUTF8CStringSize(message);
    char* cmsg = malloc(len);
    JSStringGetUTF8CString(message, cmsg, len);
    printf("\033[90mDEBUG: %s\033[0m\n", cmsg);  // Gray color for debug
    free(cmsg);
    JSStringRelease(message);
    return JSValueMakeUndefined(ctx);
}

/**
 * console.error implementation
 * @param args[0]  Error message
 */
static JSValueRef console_error(JSContextRef ctx, JSObjectRef function,
                               JSObjectRef thisObject, size_t argc,
                               const JSValueRef args[], JSValueRef* exception) {
    JSStringRef message = JSValueToStringCopy(ctx, args[0], exception);
    size_t len = JSStringGetMaximumUTF8CStringSize(message);
    char* cmsg = malloc(len);
    JSStringGetUTF8CString(message, cmsg, len);
    
    // Print error in red color to stderr
    fprintf(stderr, "\033[31mERROR: %s\033[0m\n", cmsg); 
    
    free(cmsg);
    JSStringRelease(message);
    return JSValueMakeUndefined(ctx);
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