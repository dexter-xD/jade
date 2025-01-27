#ifndef RUNTIME_H
#define RUNTIME_H

#include <JavaScriptCore/JavaScript.h>
#include <uv.h>

// JavaScript Engine
JSGlobalContextRef create_js_context();
void execute_js(JSGlobalContextRef ctx, const char* script);

// Event Loop
void init_event_loop();
void run_event_loop();
void read_file_async(const char* path, JSContextRef ctx, JSObjectRef callback);

// System APIs
void expose_system_apis(JSGlobalContextRef ctx);

#endif // RUNTIME_H