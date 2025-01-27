#include <JavaScriptCore/JavaScript.h>

// expose console.log
static JSValueRef console_log(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef args[], JSValueRef* exception) {
  JSStringRef message = JSValueToStringCopy(ctx, args[0], exception);
  size_t len = JSStringGetMaximumUTF8CStringSize(message);
  char* cmsg = malloc(len);
  JSStringGetUTF8CString(message, cmsg, len);
  printf("LOG: %s\n", cmsg);
  free(cmsg);
  JSStringRelease(message);
  return JSValueMakeUndefined(ctx);
}

void expose_system_apis(JSGlobalContextRef ctx) {
  // create console object
  JSObjectRef console = JSObjectMake(ctx, NULL, NULL);
  JSObjectRef global = JSContextGetGlobalObject(ctx);
  JSStringRef console_name = JSStringCreateWithUTF8CString("console");

  JSObjectSetProperty(ctx, global, console_name, console, kJSPropertyAttributeNone, NULL);
  JSStringRelease(console_name);

  // add console.log
  JSStringRef log_name = JSStringCreateWithUTF8CString("log");
  JSObjectRef log_func = JSObjectMakeFunctionWithCallback(ctx, log_name, console_log);
  
  JSObjectSetProperty(ctx, console, log_name, log_func, kJSPropertyAttributeNone, NULL);
  JSStringRelease(log_name);

  // TODO: expose `readFile` and other APIs
}