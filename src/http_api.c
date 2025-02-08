#include <JavaScriptCore/JavaScript.h>
#include <uv.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"

typedef struct {
    uv_tcp_t socket;
    JSContextRef ctx;
    JSObjectRef callback;
    char* host;
    char* path;
    char* response_data;
    size_t response_size;
    size_t response_len;
} HttpRequest;

void on_http_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    HttpRequest* req = (HttpRequest*)stream->data;

    if (nread > 0) {
        size_t new_len = req->response_len + nread;
        if (new_len > req->response_size) {
            req->response_size = new_len + 1;
            req->response_data = realloc(req->response_data, req->response_size);
        }
        memcpy(req->response_data + req->response_len, buf->base, nread);
        req->response_len += nread;
    } else if (nread == UV_EOF) {
        if (req->response_len > 0) {
            req->response_data = realloc(req->response_data, req->response_len + 1);
            req->response_data[req->response_len] = '\0';

            char* body = strstr(req->response_data, "\r\n\r\n");
            if (body) {
                body += 4; // Skip past headers

                // Try parsing as JSON
                JSStringRef jsResponse = JSStringCreateWithUTF8CString(body);
                JSValueRef jsonValue = JSValueMakeFromJSONString(req->ctx, jsResponse);

                JSValueRef args[2];
                args[0] = JSValueMakeNull(req->ctx); // No error

                if (jsonValue && !JSValueIsNull(req->ctx, jsonValue)) {
                    // JSON parsed successfully
                    // Convert the JSON object to a string using JSON.stringify
                    JSStringRef jsonStringifyName = JSStringCreateWithUTF8CString("JSON.stringify");
                    JSValueRef jsonStringifyFunc = JSEvaluateScript(req->ctx, jsonStringifyName, NULL, NULL, 0, NULL);
                    JSStringRelease(jsonStringifyName);

                    if (JSValueIsObject(req->ctx, jsonStringifyFunc) && JSObjectIsFunction(req->ctx, (JSObjectRef)jsonStringifyFunc)) {
                        JSValueRef jsonStringArgs[] = { jsonValue };
                        JSValueRef jsonStringValue = JSObjectCallAsFunction(req->ctx, (JSObjectRef)jsonStringifyFunc, NULL, 1, jsonStringArgs, NULL);
                        args[1] = jsonStringValue;
                    } else {
                        // Fallback: Return the raw JSON string if JSON.stringify is unavailable
                        args[1] = JSValueMakeString(req->ctx, jsResponse);
                    }
                } else {
                    // Not valid JSON, return as a string
                    args[1] = JSValueMakeString(req->ctx, jsResponse);
                }

                // Call the callback with the result
                JSObjectCallAsFunction(req->ctx, req->callback, NULL, 2, args, NULL);
                JSStringRelease(jsResponse);
            }
        }

        // Cleanup
        uv_close((uv_handle_t*)&req->socket, NULL);
        free(req->host);
        free(req->path);
        free(req->response_data);
        free(req);
    } else if (nread < 0) {
        // Handle errors
        JSStringRef errMsg = JSStringCreateWithUTF8CString(uv_strerror(nread));
        JSValueRef args[] = { JSValueMakeString(req->ctx, errMsg), JSValueMakeNull(req->ctx) };
        JSObjectCallAsFunction(req->ctx, req->callback, NULL, 2, args, NULL);
        JSStringRelease(errMsg);

        uv_close((uv_handle_t*)&req->socket, NULL);
        free(req->host);
        free(req->path);
        free(req->response_data);
        free(req);
    }

    free(buf->base); // Free the read buffer
}

void on_http_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

void on_http_write(uv_write_t* req, int status) {
    if (status < 0) fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    free(req->data);
    free(req);
}

void on_http_connect(uv_connect_t* req, int status) {
    if (status < 0) return;

    HttpRequest* http = (HttpRequest*)req->data;
    char request[1024];
    int len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n\r\n",
        http->path, http->host);

    uv_buf_t write_buf = uv_buf_init(strdup(request), len);
    uv_write_t* write_req = malloc(sizeof(uv_write_t));
    write_req->data = write_buf.base;
    uv_write(write_req, req->handle, &write_buf, 1, on_http_write);
    uv_read_start(req->handle, on_http_alloc, on_http_read);
}

void on_dns_resolved(uv_getaddrinfo_t* resolver, int status, struct addrinfo* res) {
    HttpRequest* http = (HttpRequest*)resolver->data;
    free(resolver);

    if (status < 0) {
        JSStringRef errMsg = JSStringCreateWithUTF8CString("DNS resolution failed");
        JSValueRef args[] = { JSValueMakeString(http->ctx, errMsg), JSValueMakeNull(http->ctx) };
        JSObjectCallAsFunction(http->ctx, http->callback, NULL, 2, args, NULL);
        JSStringRelease(errMsg);
        free(http->host);
        free(http->path);
        free(http);
        return;
    }

    uv_tcp_init(uv_default_loop(), &http->socket);
    http->socket.data = http;
    
    uv_connect_t* connect_req = malloc(sizeof(uv_connect_t));
    connect_req->data = http;
    uv_tcp_connect(connect_req, &http->socket, (const struct sockaddr*)res->ai_addr, on_http_connect);
    uv_freeaddrinfo(res);
}

JSValueRef http_get(JSContextRef ctx, JSObjectRef function,
                    JSObjectRef thisObject, size_t argc,
                    const JSValueRef args[], JSValueRef* exception) {
    if (argc < 2) {
        JSStringRef errMsg = JSStringCreateWithUTF8CString("http.get requires URL and callback");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    JSStringRef urlRef = JSValueToStringCopy(ctx, args[0], exception);
    size_t urlLen = JSStringGetMaximumUTF8CStringSize(urlRef);
    char* url = malloc(urlLen);
    JSStringGetUTF8CString(urlRef, url, urlLen);
    JSStringRelease(urlRef);

    if (strncmp(url, "http://", 7) != 0) {
        free(url);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Only HTTP URLs supported");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    HttpRequest* http = malloc(sizeof(HttpRequest));
    http->ctx = ctx;
    http->callback = (JSObjectRef)args[1];
    http->response_data = NULL;
    http->response_size = 0;
    http->response_len = 0;

    // Fixed URL parsing
    char* host_start = url + 7;
    char* path_start = strchr(host_start, '/');
    
    if (path_start) {
        http->host = strndup(host_start, path_start - host_start);
        http->path = strdup(path_start);
    } else {
        http->host = strdup(host_start);
        http->path = strdup("/");
    }

    if (!JSObjectIsFunction(ctx, http->callback)) {
        free(url);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Callback must be a function");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        free(http->host);
        free(http->path);
        free(http);
        return JSValueMakeUndefined(ctx);
    }

    JSValueProtect(ctx, http->callback);

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
    resolver->data = http;
    uv_getaddrinfo(uv_default_loop(), resolver, on_dns_resolved, http->host, "80", &hints);

    free(url);
    return JSValueMakeUndefined(ctx);
}