#include <uv.h>
#include "runtime.h"

uv_loop_t* loop;

void init_event_loop() {
  loop = uv_default_loop();
}

void run_event_loop() {
  uv_run(loop, UV_RUN_DEFAULT);
}

// ex: async file read
typedef struct {
  uv_fs_t req;
  JSContextRef js_ctx;
  JSObjectRef callback;
} FileReadRequest;

void on_file_read(uv_fs_t* req) {

  FileReadRequest* fr = (FileReadRequest*)req->data;
  if (req->result >= 0) {
    // call js callback with data
    JSStringRef data = JSStringCreateWithUTF8CString((char*)req->ptr);
    JSValueRef args[] = { JSValueMakeString(fr->js_ctx, data) };
    JSObjectCallAsFunction(fr->js_ctx, fr->callback, NULL, 1, args, NULL);
    JSStringRelease(data);
  }
  
  uv_fs_req_cleanup(req);
  free(fr);
}

void read_file_async(const char* path, JSContextRef ctx, JSObjectRef callback) {

  FileReadRequest* fr = malloc(sizeof(FileReadRequest));
  fr->js_ctx = ctx;
  fr->callback = callback;
  uv_fs_open(loop, &fr->req, path, O_RDONLY, 0, on_file_read);
}