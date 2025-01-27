/**
 * =====================================================================================
 * 
 *        JSC_ENGINE.C - JavaScriptCore Integration Layer
 * 
 * =====================================================================================
 * 
 * Responsible for:
 * - JavaScript context lifecycle management
 * - JS code execution
 * - Integration with system APIs
 * 
 * Key Features:
 * - Context isolation through JSGlobalContextRef
 * - Direct evaluation of JS scripts
 * - Automatic API exposure on context creation
 * 
 * Memory Management:
 * - Uses JSC's automatic garbage collection
 * - Manual string handling for JS<->C interop
 * 
 * Cross-Platform Notes:
 * - WebKitGTK required on Linux
 * - Built-in JSC framework on macOS
 * 
 * =====================================================================================
 */

#include <JavaScriptCore/JavaScript.h>
#include "runtime.h"

/**
 * Creates a fresh JS execution environment with system APIs
 * returned context must be released with JSGlobalContextRelease()
 */
JSGlobalContextRef create_js_context() {
    // Create isolated JS context
    JSGlobalContextRef ctx = JSGlobalContextCreate(NULL);
    
    // Attach system APIs to global object
    expose_system_apis(ctx);
    
    return ctx;
}

/**
 * Executes raw JS code in specified context
 * @param script  Must be UTF-8 encoded null-terminated string
 */
void execute_js(JSGlobalContextRef ctx, const char* script) {
    // Convert C string to JSC string
    JSStringRef js_code = JSStringCreateWithUTF8CString(script);
    
    // Execute script with default options
    JSEvaluateScript(ctx, js_code, NULL, NULL, 1, NULL);
    
    // Cleanup JS string resources
    JSStringRelease(js_code);
}