/**
 * =====================================================================================
 * 
 *        MAIN.C - CLI Entry Point for JavaScript Runtime
 * 
 * =====================================================================================
 * 
 * Responsibilities:
 * - Argument parsing
 * - File I/O for JS scripts
 * - Runtime initialization/cleanup
 * 
 * Execution Flow:
 * 1. Parse CLI arguments
 * 2. Read JS file
 * 3. Initialize JSC context
 * 4. Start event loop
 * 5. Execute script
 * 6. Cleanup resources
 * 
 * Error Handling:
 * - Basic file read errors
 * - Memory allocation checks (crude)
 * 
 * Future Improvements:
 * - REPL mode
 * - File watching
 * - Better error messages
 * 
 * =====================================================================================
 */

#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    // Validate arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script.js>\n", argv[0]);
        return 1;
    }

    // Read JS file
    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char* script = malloc(len + 1);
    fread(script, 1, len, f);
    script[len] = '\0';
    fclose(f);

    // Initialize runtime components
    JSGlobalContextRef ctx = create_js_context();
    init_event_loop();

    // Execute script and run event loop
    execute_js(ctx, script);
    run_event_loop();

    // Cleanup
    JSGlobalContextRelease(ctx);
    free(script);
    return 0;
}