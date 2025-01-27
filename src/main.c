#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <script.js>\n", argv[0]);
    return 1;
  }

  // read js file
  FILE* f = fopen(argv[1], "rb");
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  rewind(f);
  char* script = malloc(len + 1);
  fread(script, 1, len, f);
  script[len] = '\0';
  fclose(f);

  // init runtime
  JSGlobalContextRef ctx = create_js_context();
  init_event_loop();

  execute_js(ctx, script);
  run_event_loop();

  // cleanup
  JSGlobalContextRelease(ctx);
  free(script);
  return 0;
}