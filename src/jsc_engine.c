#include <JavaScriptCore/JavaScript.h>
#include "runtime.h"

JSGlobalContextRef create_js_context() {
  JSGlobalContextRef ctx = JSGlobalContextCreate(NULL);

  // expose APIs like console.log
  expose_system_apis(ctx);
  return ctx;
}

void execute_js(JSGlobalContextRef ctx, const char* script) {

  JSStringRef js_code = JSStringCreateWithUTF8CString(script);
  JSEvaluateScript(ctx, js_code, NULL, NULL, 1, NULL);
  JSStringRelease(js_code);
  
}