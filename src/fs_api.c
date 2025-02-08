#include <JavaScriptCore/JavaScript.h>
#include <uv.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"

// File Read Request Structure
typedef struct {
    uv_fs_t req;
    uv_file file;
    JSContextRef ctx;
    JSObjectRef callback;
    uv_buf_t buffer;
} FileReadRequest;

// File Write Request Structure
typedef struct {
    uv_fs_t req;
    uv_file file;
    JSContextRef ctx;
    JSObjectRef callback;
    uv_buf_t buffer;
} FileWriteRequest;


// Read Callback Function
void on_file_read(uv_fs_t* req) {
    FileReadRequest* fr = (FileReadRequest*)req->data;
    uv_fs_req_cleanup(req);

    // Ensure valid request
    if (!fr) {
        fprintf(stderr, "Error: FileReadRequest is NULL\n");
        return;
    }

    // Handle read errors
    if (req->result < 0) {
        fprintf(stderr, "fs.readFile() error: %s\n", uv_strerror(req->result));
        JSStringRef errMsg = JSStringCreateWithUTF8CString(uv_strerror(req->result));
        JSValueRef args[] = { JSValueMakeString(fr->ctx, errMsg), JSValueMakeNull(fr->ctx) };
        JSObjectCallAsFunction(fr->ctx, fr->callback, NULL, 2, args, NULL);
        JSStringRelease(errMsg);
    } else {
        // Convert buffer to JS string
        fr->buffer.base[req->result] = '\0'; // Ensure null termination
        JSStringRef fileData = JSStringCreateWithUTF8CString(fr->buffer.base);
        JSValueRef args[] = { JSValueMakeNull(fr->ctx), JSValueMakeString(fr->ctx, fileData) };
        JSObjectCallAsFunction(fr->ctx, fr->callback, NULL, 2, args, NULL);
        JSStringRelease(fileData);
    }

    // Cleanup
    uv_fs_close(uv_default_loop(), &fr->req, fr->file, NULL);
    JSValueUnprotect(fr->ctx, fr->callback);
    free(fr->buffer.base);
    free(fr);
}

// Open File Callback
void on_file_open(uv_fs_t* req) {
    FileReadRequest* fr = (FileReadRequest*)req->data;

    if (!fr) {
        fprintf(stderr, "Error: FileReadRequest is NULL in on_file_open()\n");
        return;
    }

    if (req->result < 0) {
        // Print error and return error to callback
        fprintf(stderr, "fs.readFile() error: %s\n", uv_strerror(req->result));
        JSStringRef errMsg = JSStringCreateWithUTF8CString(uv_strerror(req->result));
        JSValueRef args[] = { JSValueMakeString(fr->ctx, errMsg), JSValueMakeNull(fr->ctx) };
        JSObjectCallAsFunction(fr->ctx, fr->callback, NULL, 2, args, NULL);
        JSStringRelease(errMsg);
        JSValueUnprotect(fr->ctx, fr->callback);
        free(fr);
        return;
    }

    fr->file = req->result;

    // Allocate buffer
    fr->buffer = uv_buf_init((char*)malloc(1024), 1024);
    if (!fr->buffer.base) {
        fprintf(stderr, "Memory allocation failed\n");
        uv_fs_close(uv_default_loop(), &fr->req, fr->file, NULL);
        JSValueUnprotect(fr->ctx, fr->callback);
        free(fr);
        return;
    }

    // Read file asynchronously
    uv_fs_read(uv_default_loop(), &fr->req, fr->file, &fr->buffer, 1, 0, on_file_read);
}

// `fs.readFile(path, callback)`
JSValueRef fs_read_file(JSContextRef ctx, JSObjectRef function,
                        JSObjectRef thisObject, size_t argc,
                        const JSValueRef args[], JSValueRef* exception) {
    if (argc < 2) {
        JSStringRef errMsg = JSStringCreateWithUTF8CString("fs.readFile requires a path and callback");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    // Convert JS string (path)
    JSStringRef pathRef = JSValueToStringCopy(ctx, args[0], exception);
    size_t pathLen = JSStringGetMaximumUTF8CStringSize(pathRef);
    char* path = (char*)malloc(pathLen);
    if (!path) {
        JSStringRelease(pathRef);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Memory allocation failed");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }
    JSStringGetUTF8CString(pathRef, path, pathLen);
    JSStringRelease(pathRef);

    // Debug print
    printf("Opening file: %s\n", path);

    // Ensure callback is a function
    if (!JSValueIsObject(ctx, args[1]) || !JSObjectIsFunction(ctx, (JSObjectRef)args[1])) {
        free(path);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Second argument must be a function");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    // Create File Read Request
    FileReadRequest* fr = (FileReadRequest*)malloc(sizeof(FileReadRequest));
    if (!fr) {
        free(path);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Memory allocation failed");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    fr->ctx = ctx;
    fr->callback = (JSObjectRef)args[1];
    JSValueProtect(ctx, fr->callback);

    // Open File Asynchronously
    uv_fs_open(uv_default_loop(), &fr->req, path, O_RDONLY, 0, on_file_open);
    fr->req.data = fr;
    free(path);

    return JSValueMakeUndefined(ctx);
}


// Write Callback Function
void on_file_write(uv_fs_t* req) {
    FileWriteRequest* fw = (FileWriteRequest*)req->data;
    uv_fs_req_cleanup(req);

    // Ensure valid request
    if (!fw) {
        fprintf(stderr, "Error: FileWriteRequest is NULL\n");
        return;
    }

    // Handle write errors
    if (req->result < 0) {
        fprintf(stderr, "fs.writeFile() error: %s\n", uv_strerror(req->result));
        JSStringRef errMsg = JSStringCreateWithUTF8CString(uv_strerror(req->result));
        JSValueRef args[] = { JSValueMakeString(fw->ctx, errMsg) };
        JSObjectCallAsFunction(fw->ctx, fw->callback, NULL, 1, args, NULL);
        JSStringRelease(errMsg);
    } else {
        // Call JS callback with no error
        JSValueRef args[] = { JSValueMakeNull(fw->ctx) };
        JSObjectCallAsFunction(fw->ctx, fw->callback, NULL, 1, args, NULL);
    }

    // Cleanup
    uv_fs_close(uv_default_loop(), &fw->req, fw->file, NULL);
    JSValueUnprotect(fw->ctx, fw->callback);
    free(fw->buffer.base);
    free(fw);
}

// Open File Callback
void on_file_open_write(uv_fs_t* req) {
    FileWriteRequest* fw = (FileWriteRequest*)req->data;

    if (!fw) {
        fprintf(stderr, "Error: FileWriteRequest is NULL in on_file_open_write()\n");
        return;
    }

    if (req->result < 0) {
        // Print error and return error to callback
        fprintf(stderr, "fs.writeFile() error: %s\n", uv_strerror(req->result));
        JSStringRef errMsg = JSStringCreateWithUTF8CString(uv_strerror(req->result));
        JSValueRef args[] = { JSValueMakeString(fw->ctx, errMsg) };
        JSObjectCallAsFunction(fw->ctx, fw->callback, NULL, 1, args, NULL);
        JSStringRelease(errMsg);
        JSValueUnprotect(fw->ctx, fw->callback);
        free(fw);
        return;
    }

    fw->file = req->result;

    // Write file asynchronously
    uv_fs_write(uv_default_loop(), &fw->req, fw->file, &fw->buffer, 1, 0, on_file_write);
}

// `fs.writeFile(path, content, callback)`
JSValueRef fs_write_file(JSContextRef ctx, JSObjectRef function,
                         JSObjectRef thisObject, size_t argc,
                         const JSValueRef args[], JSValueRef* exception) {
    if (argc < 3) {
        JSStringRef errMsg = JSStringCreateWithUTF8CString("fs.writeFile requires a path, content, and callback");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    // Convert JS string (path)
    JSStringRef pathRef = JSValueToStringCopy(ctx, args[0], exception);
    size_t pathLen = JSStringGetMaximumUTF8CStringSize(pathRef);
    char* path = (char*)malloc(pathLen);
    if (!path) {
        JSStringRelease(pathRef);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Memory allocation failed");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }
    JSStringGetUTF8CString(pathRef, path, pathLen);
    JSStringRelease(pathRef);

    // Convert JS string (content)
    JSStringRef contentRef = JSValueToStringCopy(ctx, args[1], exception);
    size_t contentLen = JSStringGetMaximumUTF8CStringSize(contentRef);
    char* content = (char*)malloc(contentLen);
    if (!content) {
        free(path);
        JSStringRelease(contentRef);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Memory allocation failed");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }
    JSStringGetUTF8CString(contentRef, content, contentLen);
    JSStringRelease(contentRef);

    // Ensure callback is a function
    if (!JSValueIsObject(ctx, args[2]) || !JSObjectIsFunction(ctx, (JSObjectRef)args[2])) {
        free(path);
        free(content);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Third argument must be a function");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    // Create File Write Request
    FileWriteRequest* fw = (FileWriteRequest*)malloc(sizeof(FileWriteRequest));
    if (!fw) {
        free(path);
        free(content);
        JSStringRef errMsg = JSStringCreateWithUTF8CString("Memory allocation failed");
        *exception = JSValueMakeString(ctx, errMsg);
        JSStringRelease(errMsg);
        return JSValueMakeUndefined(ctx);
    }

    fw->ctx = ctx;
    fw->callback = (JSObjectRef)args[2];
    JSValueProtect(ctx, fw->callback);
    fw->buffer = uv_buf_init(content, strlen(content));

    // Open File Asynchronously (Create/Truncate mode)
    uv_fs_open(uv_default_loop(), &fw->req, path, O_WRONLY | O_CREAT | O_TRUNC, 0644, on_file_open_write);
    fw->req.data = fw;
    free(path);

    return JSValueMakeUndefined(ctx);
}
