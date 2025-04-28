#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <uv.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"

// ========================= HTTP CLIENT (http.get) ========================= //

typedef struct {
    uv_tcp_t socket;
    JSContextRef ctx;
    JSObjectRef callback;
    char* host;
    char* path;
    char* response_data;
    size_t response_size;
    size_t response_len;
    int status_code;
    char* response_headers;
    size_t headers_size;
    size_t headers_len;
    bool headers_parsed;
    char* response_body;
    size_t body_size;
    size_t body_len;
    char* request_data;    // Added for POST data
    size_t request_data_len; // Added for POST data length
    const char* method;  // Added for HTTP method
} HttpRequest;

// Forward declarations
void on_write_complete(uv_write_t* req, int status);
void on_http_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void on_http_connect(uv_connect_t* req, int status);
void on_dns_resolved(uv_getaddrinfo_t* resolver, int status, struct addrinfo* res);
JSValueRef res_end(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                   size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

// Write callback for uv_write
void on_write_complete(uv_write_t* req, int status) {
    free(req->data); // Free the buffer data
    free(req);       // Free the write request
}

// Allocate Buffer
void on_http_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

// Read Callback
void on_http_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    HttpRequest* req = (HttpRequest*)stream->data;

    if (nread > 0) {
        // Append to response data
        req->response_data = realloc(req->response_data, req->response_size + nread + 1);
        memcpy(req->response_data + req->response_size, buf->base, nread);
        req->response_size += nread;
        req->response_data[req->response_size] = '\0';

        // Parse response if headers haven't been parsed yet
        if (!req->headers_parsed) {
            char* header_end = strstr(req->response_data, "\r\n\r\n");
            if (header_end) {
                // Calculate header length
                size_t header_len = header_end - req->response_data;
                
                // Store headers
                req->response_headers = malloc(header_len + 1);
                memcpy(req->response_headers, req->response_data, header_len);
                req->response_headers[header_len] = '\0';
                req->headers_size = header_len;
                req->headers_len = header_len;

                // Parse status code
                char* status_line = strtok(req->response_headers, "\r\n");
                if (status_line) {
                    sscanf(status_line, "HTTP/1.1 %d", &req->status_code);
                }

                // Store body
                size_t body_start = header_len + 4; // Skip \r\n\r\n
                size_t body_len = req->response_size - body_start;
                if (body_len > 0) {  // Only allocate if there's a body
                    req->response_body = malloc(body_len + 1);
                    memcpy(req->response_body, req->response_data + body_start, body_len);
                    req->response_body[body_len] = '\0';
                    req->body_size = body_len;
                    req->body_len = body_len;
                } else {
                    req->response_body = strdup("{}");  // Empty JSON object for empty body
                    req->body_size = 2;
                    req->body_len = 2;
                }

                req->headers_parsed = true;
            }
        }
    } else {
        // Create response object
        JSObjectRef responseObj = JSObjectMake(req->ctx, NULL, NULL);
        
        // Add status code
        JSStringRef statusName = JSStringCreateWithUTF8CString("statusCode");
        JSObjectSetProperty(req->ctx, responseObj, statusName, 
                           JSValueMakeNumber(req->ctx, req->status_code), 
                           kJSPropertyAttributeNone, NULL);
        JSStringRelease(statusName);

        // Add headers
        JSObjectRef headersObj = JSObjectMake(req->ctx, NULL, NULL);
        if (req->response_headers) {
            char* header_line = strtok(req->response_headers, "\r\n");
            while (header_line) {
                char* colon = strchr(header_line, ':');
                if (colon) {
                    *colon = '\0';
                    char* value = colon + 1;
                    while (*value == ' ') value++; // Skip leading spaces

                    JSStringRef headerName = JSStringCreateWithUTF8CString(header_line);
                    JSStringRef headerValue = JSStringCreateWithUTF8CString(value);
                    JSObjectSetProperty(req->ctx, headersObj, headerName,
                                      JSValueMakeString(req->ctx, headerValue),
                                      kJSPropertyAttributeNone, NULL);
                    JSStringRelease(headerName);
                    JSStringRelease(headerValue);
                }
                header_line = strtok(NULL, "\r\n");
            }
        }
        JSStringRef headersName = JSStringCreateWithUTF8CString("headers");
        JSObjectSetProperty(req->ctx, responseObj, headersName,
                           JSValueToObject(req->ctx, headersObj, NULL),
                           kJSPropertyAttributeNone, NULL);
        JSStringRelease(headersName);

        // Add body
        JSStringRef bodyName = JSStringCreateWithUTF8CString("body");
        JSStringRef bodyValue = JSStringCreateWithUTF8CString(req->response_body ? req->response_body : "{}");
        JSObjectSetProperty(req->ctx, responseObj, bodyName,
                           JSValueMakeString(req->ctx, bodyValue),
                           kJSPropertyAttributeNone, NULL);
        JSStringRelease(bodyName);
        JSStringRelease(bodyValue);

        // Call the JS callback with response
        JSValueRef args[] = { JSValueMakeNull(req->ctx), JSValueToObject(req->ctx, responseObj, NULL) };
        JSObjectCallAsFunction(req->ctx, req->callback, NULL, 2, args, NULL);

        // Cleanup
        uv_close((uv_handle_t*)&req->socket, NULL);
        free(req->host);
        free(req->path);
        free(req->response_data);
        free(req->response_headers);
        free(req->response_body);
        free(req);
    }

    free(buf->base);
}

// Helper function to check if data is JSON
static bool is_json_data(const char* data) {
    if (!data) return false;
    // Simple check: starts with { and ends with }
    return data[0] == '{' && data[strlen(data) - 1] == '}';
}

// Modify the connect callback to handle different content types
void on_http_connect(uv_connect_t* req, int status) {
    if (status < 0) return;

    HttpRequest* http = (HttpRequest*)req->data;
    char request[1024];
    
    // Check the HTTP method
    if (http->method && strcmp(http->method, "DELETE") == 0) {
        snprintf(request, sizeof(request),
            "DELETE %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n\r\n",
            http->path, http->host);
    } else if (http->method && strcmp(http->method, "PUT") == 0) {
        const char* content_type = is_json_data(http->request_data) ? 
            "application/json" : "application/x-www-form-urlencoded";
            
        snprintf(request, sizeof(request),
            "PUT %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n"
            "%s",
            http->path, http->host, content_type, http->request_data_len, http->request_data);
    } else if (http->request_data) {
        const char* content_type = is_json_data(http->request_data) ? 
            "application/json" : "application/x-www-form-urlencoded";
            
        snprintf(request, sizeof(request),
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n"
            "%s",
            http->path, http->host, content_type, http->request_data_len, http->request_data);
    } else {
        snprintf(request, sizeof(request),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n\r\n",
            http->path, http->host);
    }

    uv_buf_t write_buf = uv_buf_init(strdup(request), strlen(request));
    uv_write_t* write_req = malloc(sizeof(uv_write_t));
    write_req->data = write_buf.base;
    uv_write(write_req, req->handle, &write_buf, 1, on_write_complete);
    uv_read_start(req->handle, on_http_alloc, on_http_read);
}

// DNS Resolution Callback
void on_dns_resolved(uv_getaddrinfo_t* resolver, int status, struct addrinfo* res) {
    HttpRequest* http = (HttpRequest*)resolver->data;
    free(resolver);

    if (status < 0) {
        return;
    }

    uv_tcp_init(uv_default_loop(), &http->socket);
    http->socket.data = http;

    uv_connect_t* connect_req = malloc(sizeof(uv_connect_t));
    connect_req->data = http;
    uv_tcp_connect(connect_req, &http->socket, (const struct sockaddr*)res->ai_addr, on_http_connect);
    uv_freeaddrinfo(res);
}

// `http.get(url, callback)`
JSValueRef http_get(JSContextRef ctx, JSObjectRef function,
                    JSObjectRef thisObject, size_t argc,
                    const JSValueRef args[], JSValueRef* exception) {
    if (argc < 2) return JSValueMakeUndefined(ctx);

    // Parse URL
    JSStringRef urlRef = JSValueToStringCopy(ctx, args[0], exception);
    size_t urlLen = JSStringGetMaximumUTF8CStringSize(urlRef);
    char* url = malloc(urlLen);
    JSStringGetUTF8CString(urlRef, url, urlLen);
    JSStringRelease(urlRef);

    // Ensure HTTP scheme
    if (strncmp(url, "http://", 7) != 0) {
        free(url);
        return JSValueMakeUndefined(ctx);
    }

    // Parse host and path
    char* host_start = url + 7;
    char* path_start = strchr(host_start, '/');
    char* host = path_start ? strndup(host_start, path_start - host_start) : strdup(host_start);
    char* path = path_start ? strdup(path_start) : strdup("/");

    // Setup HTTP request
    HttpRequest* http = malloc(sizeof(HttpRequest));
    http->ctx = ctx;
    http->callback = (JSObjectRef)args[1];
    http->host = host;
    http->path = path;
    http->response_data = NULL;
    http->response_size = 0;
    http->response_len = 0;
    http->status_code = 0;
    http->response_headers = NULL;
    http->headers_size = 0;
    http->headers_len = 0;
    http->headers_parsed = false;
    http->response_body = NULL;
    http->body_size = 0;
    http->body_len = 0;
    http->request_data = NULL;
    http->request_data_len = 0;
    http->method = NULL;  // Set the HTTP method to NULL

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
    resolver->data = http;
    uv_getaddrinfo(uv_default_loop(), resolver, on_dns_resolved, http->host, "80", &hints);

    free(url);
    return JSValueMakeUndefined(ctx);
}

// `http.post(url, data, callback)`
JSValueRef http_post(JSContextRef ctx, JSObjectRef function,
                    JSObjectRef thisObject, size_t argc,
                    const JSValueRef args[], JSValueRef* exception) {
    if (argc < 3) {
        JSStringRef msg = JSStringCreateWithUTF8CString("http.post requires 3 arguments: url, data, and callback");
        *exception = JSValueMakeString(ctx, msg);
        JSStringRelease(msg);
        return JSValueMakeUndefined(ctx);
    }

    // Parse URL
    JSStringRef urlRef = JSValueToStringCopy(ctx, args[0], exception);
    size_t urlLen = JSStringGetMaximumUTF8CStringSize(urlRef);
    char* url = malloc(urlLen);
    JSStringGetUTF8CString(urlRef, url, urlLen);
    JSStringRelease(urlRef);

    // Ensure HTTP scheme
    if (strncmp(url, "http://", 7) != 0) {
        free(url);
        return JSValueMakeUndefined(ctx);
    }

    // Parse host and path
    char* host_start = url + 7;
    char* path_start = strchr(host_start, '/');
    char* host = path_start ? strndup(host_start, path_start - host_start) : strdup(host_start);
    char* path = path_start ? strdup(path_start) : strdup("/");

    // Parse data
    JSStringRef dataRef = JSValueToStringCopy(ctx, args[1], exception);
    size_t dataLen = JSStringGetMaximumUTF8CStringSize(dataRef);
    char* data = malloc(dataLen);
    JSStringGetUTF8CString(dataRef, data, dataLen);
    JSStringRelease(dataRef);

    // Setup HTTP request
    HttpRequest* http = malloc(sizeof(HttpRequest));
    http->ctx = ctx;
    http->callback = (JSObjectRef)args[2];
    http->host = host;
    http->path = path;
    http->response_data = NULL;
    http->response_size = 0;
    http->response_len = 0;
    http->status_code = 0;
    http->response_headers = NULL;
    http->headers_size = 0;
    http->headers_len = 0;
    http->headers_parsed = false;
    http->response_body = NULL;
    http->body_size = 0;
    http->body_len = 0;
    http->request_data = data;
    http->request_data_len = strlen(data);
    http->method = "POST";  // Set the HTTP method

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
    resolver->data = http;
    uv_getaddrinfo(uv_default_loop(), resolver, on_dns_resolved, http->host, "80", &hints);

    free(url);
    return JSValueMakeUndefined(ctx);
}

// `http.put(url, data, callback)`
JSValueRef http_put(JSContextRef ctx, JSObjectRef function,
                   JSObjectRef thisObject, size_t argc,
                   const JSValueRef args[], JSValueRef* exception) {
    if (argc < 3) {
        JSStringRef msg = JSStringCreateWithUTF8CString("http.put requires 3 arguments: url, data, and callback");
        *exception = JSValueMakeString(ctx, msg);
        JSStringRelease(msg);
        return JSValueMakeUndefined(ctx);
    }

    // Parse URL
    JSStringRef urlRef = JSValueToStringCopy(ctx, args[0], exception);
    size_t urlLen = JSStringGetMaximumUTF8CStringSize(urlRef);
    char* url = malloc(urlLen);
    JSStringGetUTF8CString(urlRef, url, urlLen);
    JSStringRelease(urlRef);

    // Ensure HTTP scheme
    if (strncmp(url, "http://", 7) != 0) {
        free(url);
        return JSValueMakeUndefined(ctx);
    }

    // Parse host and path
    char* host_start = url + 7;
    char* path_start = strchr(host_start, '/');
    char* host = path_start ? strndup(host_start, path_start - host_start) : strdup(host_start);
    char* path = path_start ? strdup(path_start) : strdup("/");

    // Parse data
    JSStringRef dataRef = JSValueToStringCopy(ctx, args[1], exception);
    size_t dataLen = JSStringGetMaximumUTF8CStringSize(dataRef);
    char* data = malloc(dataLen);
    JSStringGetUTF8CString(dataRef, data, dataLen);
    JSStringRelease(dataRef);

    // Setup HTTP request
    HttpRequest* http = malloc(sizeof(HttpRequest));
    http->ctx = ctx;
    http->callback = (JSObjectRef)args[2];
    http->host = host;
    http->path = path;
    http->response_data = NULL;
    http->response_size = 0;
    http->response_len = 0;
    http->status_code = 0;
    http->response_headers = NULL;
    http->headers_size = 0;
    http->headers_len = 0;
    http->headers_parsed = false;
    http->response_body = NULL;
    http->body_size = 0;
    http->body_len = 0;
    http->request_data = data;
    http->request_data_len = strlen(data);
    http->method = "PUT";  // Set the HTTP method

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
    resolver->data = http;
    uv_getaddrinfo(uv_default_loop(), resolver, on_dns_resolved, http->host, "80", &hints);

    free(url);
    return JSValueMakeUndefined(ctx);
}

// `http.delete(url, callback)`
JSValueRef http_delete(JSContextRef ctx, JSObjectRef function,
                      JSObjectRef thisObject, size_t argc,
                      const JSValueRef args[], JSValueRef* exception) {
    if (argc < 2) {
        JSStringRef msg = JSStringCreateWithUTF8CString("http.delete requires 2 arguments: url and callback");
        *exception = JSValueMakeString(ctx, msg);
        JSStringRelease(msg);
        return JSValueMakeUndefined(ctx);
    }

    // Parse URL
    JSStringRef urlRef = JSValueToStringCopy(ctx, args[0], exception);
    size_t urlLen = JSStringGetMaximumUTF8CStringSize(urlRef);
    char* url = malloc(urlLen);
    JSStringGetUTF8CString(urlRef, url, urlLen);
    JSStringRelease(urlRef);

    // Ensure HTTP scheme
    if (strncmp(url, "http://", 7) != 0) {
        free(url);
        return JSValueMakeUndefined(ctx);
    }

    // Parse host and path
    char* host_start = url + 7;
    char* path_start = strchr(host_start, '/');
    char* host = path_start ? strndup(host_start, path_start - host_start) : strdup(host_start);
    char* path = path_start ? strdup(path_start) : strdup("/");

    // Setup HTTP request
    HttpRequest* http = malloc(sizeof(HttpRequest));
    http->ctx = ctx;
    http->callback = (JSObjectRef)args[1];
    http->host = host;
    http->path = path;
    http->response_data = NULL;
    http->response_size = 0;
    http->response_len = 0;
    http->status_code = 0;
    http->response_headers = NULL;
    http->headers_size = 0;
    http->headers_len = 0;
    http->headers_parsed = false;
    http->response_body = NULL;
    http->body_size = 0;
    http->body_len = 0;
    http->request_data = NULL;
    http->request_data_len = 0;
    http->method = "DELETE";  // Set the HTTP method

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    uv_getaddrinfo_t* resolver = malloc(sizeof(uv_getaddrinfo_t));
    resolver->data = http;
    uv_getaddrinfo(uv_default_loop(), resolver, on_dns_resolved, http->host, "80", &hints);

    free(url);
    return JSValueMakeUndefined(ctx);
}

// ========================= HTTP SERVER (http.createServer) ========================= //

typedef struct {
    uv_tcp_t server;
    JSContextRef ctx;
    JSObjectRef callback;
} HttpServer;

// Structure to track client connections
typedef struct {
    uv_tcp_t* handle;
    HttpServer* server;
    JSObjectRef req;
    JSObjectRef res;
} ClientContext;

// Finalize callback for the server object
static void server_finalize(JSObjectRef object) {
    HttpServer* server = (HttpServer*)JSObjectGetPrivate(object);
    if (server) {
        uv_close((uv_handle_t*)&server->server, NULL);
        free(server);
    }
}

// Response "end" method implementation
JSValueRef res_end(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                   size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    ClientContext* clientCtx = (ClientContext*)JSObjectGetPrivate(thisObject);
    if (!clientCtx) return JSValueMakeUndefined(ctx);

    const char* body = "Hello, World!";
    char response[512];
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n%s",
        strlen(body), body);

    uv_buf_t buf = uv_buf_init(strdup(response), strlen(response));
    uv_write_t* write_req = malloc(sizeof(uv_write_t));
    write_req->data = buf.base;
    uv_write(write_req, (uv_stream_t*)clientCtx->handle, &buf, 1, on_write_complete);

    return JSValueMakeUndefined(ctx);
}

// Read callback for client data
void on_client_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    ClientContext* clientCtx = (ClientContext*)client->data;

    if (nread > 0) {
        buf->base[nread] = '\0'; // Ensure null-terminated string

        char method[16], url[256];

        // Extract only the first line of the request
        char* first_line = strtok(buf->base, "\r\n");
        if (first_line && sscanf(first_line, "%15s %255s", method, url) == 2) {
            // Convert method & URL to JavaScript strings
            JSStringRef methodRef = JSStringCreateWithUTF8CString(method);
            JSStringRef urlRef = JSStringCreateWithUTF8CString(url);

            // Set req.method
            JSStringRef methodName = JSStringCreateWithUTF8CString("method");
            JSObjectSetProperty(clientCtx->server->ctx, clientCtx->req, methodName, JSValueMakeString(clientCtx->server->ctx, methodRef), kJSPropertyAttributeNone, NULL);
            JSStringRelease(methodRef);
            JSStringRelease(methodName);

            // Set req.url
            JSStringRef urlName = JSStringCreateWithUTF8CString("url");
            JSObjectSetProperty(clientCtx->server->ctx, clientCtx->req, urlName, JSValueMakeString(clientCtx->server->ctx, urlRef), kJSPropertyAttributeNone, NULL);
            JSStringRelease(urlRef);
            JSStringRelease(urlName);

            printf("LOG: Received %s request for %s\n", method, url);
        } else {
            fprintf(stderr, "ERROR: Failed to parse HTTP request\n");
        }
    }

    free(buf->base);
}

// Handle new HTTP connections
void on_new_http_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf(stderr, "ERROR: New connection failed: %s\n", uv_strerror(status));
        return;
    }

    HttpServer* httpServer = (HttpServer*)server->data;
    uv_tcp_t* client = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);

    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        ClientContext* clientCtx = malloc(sizeof(ClientContext));
        clientCtx->handle = client;
        clientCtx->server = httpServer;

        // Create req/res objects
        clientCtx->req = JSObjectMake(httpServer->ctx, NULL, NULL);
        clientCtx->res = JSObjectMake(httpServer->ctx, NULL, NULL);

        // Add "end" method to res
        JSStringRef endName = JSStringCreateWithUTF8CString("end");
        JSObjectRef endFunc = JSObjectMakeFunctionWithCallback(httpServer->ctx, endName, res_end);
        JSObjectSetProperty(httpServer->ctx, clientCtx->res, endName, endFunc, kJSPropertyAttributeNone, NULL);
        JSStringRelease(endName);

        // Associate client context with res object
        JSObjectSetPrivate(clientCtx->res, clientCtx);

        // Start reading client data
        client->data = clientCtx;
        uv_read_start((uv_stream_t*)client, on_http_alloc, on_client_read);

        // Call the JS callback with req and res
        JSValueRef args[] = { clientCtx->req, clientCtx->res };
        JSObjectCallAsFunction(httpServer->ctx, httpServer->callback, NULL, 2, args, NULL);
    } else {
        fprintf(stderr, "ERROR: Failed to accept client connection\n");
        free(client);
    }
}

// `http.createServer(callback)`
JSValueRef http_create_server(JSContextRef ctx, JSObjectRef function,
                              JSObjectRef thisObject, size_t argc,
                              const JSValueRef args[], JSValueRef* exception) {
    if (argc < 1) return JSValueMakeUndefined(ctx);

    // Create the HttpServer structure
    HttpServer* server = malloc(sizeof(HttpServer));
    server->ctx = ctx;
    server->callback = (JSObjectRef)args[0];

    // Initialize the TCP server
    uv_tcp_init(uv_default_loop(), &server->server);
    server->server.data = server;

    // Define a JavaScript class for the server object
    JSClassDefinition serverClassDef = kJSClassDefinitionEmpty;
    serverClassDef.finalize = server_finalize; // Use the C function for finalization
    JSClassRef serverClass = JSClassCreate(&serverClassDef);
    JSObjectRef serverObject = JSObjectMake(ctx, serverClass, server);
    JSClassRelease(serverClass);

    // Add the `listen` method to the server object
    JSStringRef listenName = JSStringCreateWithUTF8CString("listen");
    JSObjectRef listenFunc = JSObjectMakeFunctionWithCallback(ctx, listenName, http_server_listen);
    JSObjectSetProperty(ctx, serverObject, listenName, listenFunc, kJSPropertyAttributeNone, NULL);
    JSStringRelease(listenName);

    return serverObject;
}

// `server.listen(port)`
JSValueRef http_server_listen(JSContextRef ctx, JSObjectRef function,
                              JSObjectRef thisObject, size_t argc,
                              const JSValueRef args[], JSValueRef* exception) {
    if (argc < 1 || !JSValueIsNumber(ctx, args[0])) {
        fprintf(stderr, "ERROR: server.listen() requires a valid port number\n");
        return JSValueMakeUndefined(ctx);
    }

    // Retrieve the HttpServer object
    HttpServer* server = (HttpServer*)JSObjectGetPrivate(thisObject);
    if (!server) {
        fprintf(stderr, "ERROR: Invalid server object\n");
        return JSValueMakeUndefined(ctx);
    }

    int port = (int)JSValueToNumber(ctx, args[0], exception);
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    // Bind the server to the specified port
    int bind_result = uv_tcp_bind(&server->server, (const struct sockaddr*)&addr, 0);
    if (bind_result < 0) {
        fprintf(stderr, "ERROR: Failed to bind to port %d: %s\n", port, uv_strerror(bind_result));
        return JSValueMakeUndefined(ctx);
    }

    // Start listening for incoming connections
    int listen_result = uv_listen((uv_stream_t*)&server->server, 10, on_new_http_connection);
    if (listen_result < 0) {
        fprintf(stderr, "ERROR: HTTP server failed to listen on port %d: %s\n", port, uv_strerror(listen_result));
        return JSValueMakeUndefined(ctx);
    }

    printf("LOG: HTTP Server listening on port %d\n", port);
    return JSValueMakeUndefined(ctx);
}