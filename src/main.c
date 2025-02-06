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
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variables for command-line arguments
int process_argc;
char** process_argv;

// Function to print version information
void print_version() {
    printf("Jade Runtime v%s\n", RUNTIME_VERSION);
}

// Function to print help/usage information
void print_help() {
    printf("Usage: jade [options] [script.js]\n");
    printf("Options:\n");
    printf("  --version   Print version\n");
    printf("  --help      Show help\n");
    printf("  --eval <code> Execute inline code\n");
}


int main(int argc, char** argv) {
    process_argc = argc;
    process_argv = argv;
    char* eval_code = NULL;
    char* script_file = NULL;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "--eval") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --eval requires code argument\n");
                return 1;
            }
            eval_code = argv[i + 1];
            i++; // Skip code argument
        } else {
            script_file = argv[i];
            break;
        }
    }

    // Handle --eval
    if (eval_code != NULL) {
        JSGlobalContextRef ctx = create_js_context();
        init_event_loop();
        execute_js(ctx, eval_code);
        run_event_loop();
        JSGlobalContextRelease(ctx);
        return 0;
    }

    // Handle script file
    if (!script_file) {
        fprintf(stderr, "Error: No script or --eval provided\n");
        print_help();
        return 1;
    }

    // Read JS file
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
        return 1;
    }
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