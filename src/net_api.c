#include <JavaScriptCore/JavaScript.h>
#include <uv.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"

// Define a class to store ServerRequest*
static JSClassRef serverClass = NULL;

// TCP Server Request Structure
typedef struct {
    uv_tcp_t server;
    JSContextRef ctx;
    JSObjectRef callback;
} ServerRequest;

// Destructor for the server object
void server_finalize(JSObjectRef object) {
    ServerRequest* sr = (ServerRequest*)JSObjectGetPrivate(object);
    if (sr) {
        JSValueUnprotect(sr->ctx, sr->callback);
        free(sr);
    }
}

// Client Connection Callback
void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) return;

    ServerRequest* sr = (ServerRequest*)server->data;

    // Accept client connection
    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);
    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        // Call JavaScript callback
        JSValueRef args[] = { JSValueMakeNumber(sr->ctx, (uintptr_t)client) };
        JSObjectCallAsFunction(sr->ctx, sr->callback, NULL, 1, args, NULL);
    } else {
        uv_close((uv_handle_t*)client, NULL);
    }
}

// `net.createServer(callback)`
JSValueRef net_create_server(JSContextRef ctx, JSObjectRef function,
                             JSObjectRef thisObject, size_t argc,
                             const JSValueRef args[], JSValueRef* exception) {
    if (argc < 1 || !JSValueIsObject(ctx, args[0]) || !JSObjectIsFunction(ctx, (JSObjectRef)args[0])) {
        JSStringRef errMsg = JSStringCreateWithUTF8CString("net.createServer requires a callback function");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    // Create ServerRequest struct
    ServerRequest* sr = (ServerRequest*)malloc(sizeof(ServerRequest));
    if (!sr) return JSValueMakeUndefined(ctx);

    sr->ctx = ctx;
    sr->callback = (JSObjectRef)args[0];
    JSValueProtect(ctx, sr->callback);

    // Initialize TCP server
    uv_tcp_init(uv_default_loop(), &sr->server);
    sr->server.data = sr;

    // Create a class definition for the server object (only once)
    if (serverClass == NULL) {
        JSClassDefinition classDef = kJSClassDefinitionEmpty;
        classDef.finalize = server_finalize;
        serverClass = JSClassCreate(&classDef);
    }

    // Create a proper JS object for the server
    JSObjectRef serverObject = JSObjectMake(ctx, serverClass, sr);

    // Attach `server.listen(port)`
    JSStringRef listenName = JSStringCreateWithUTF8CString("listen");
    JSObjectRef listenFunc = JSObjectMakeFunctionWithCallback(ctx, listenName, net_server_listen);
    JSObjectSetProperty(ctx, serverObject, listenName, listenFunc, kJSPropertyAttributeNone, NULL);
    JSStringRelease(listenName);

    return serverObject;
}

// `server.listen(port)`
JSValueRef net_server_listen(JSContextRef ctx, JSObjectRef function,
                             JSObjectRef thisObject, size_t argc,
                             const JSValueRef args[], JSValueRef* exception) {
    if (argc < 1 || !JSValueIsNumber(ctx, args[0])) return JSValueMakeUndefined(ctx);

    ServerRequest* sr = (ServerRequest*)JSObjectGetPrivate(thisObject);
    if (!sr) return JSValueMakeUndefined(ctx);

    int port = (int)JSValueToNumber(ctx, args[0], exception);
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    uv_tcp_bind(&sr->server, (const struct sockaddr*)&addr, 0);
    int result = uv_listen((uv_stream_t*)&sr->server, 10, on_new_connection);
    if (result < 0) {
        fprintf(stderr, "Server listen error: %s\n", uv_strerror(result));
    }

    return JSValueMakeUndefined(ctx);
}
