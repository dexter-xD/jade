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
    printf("Usage: jade [options] <script.js>\n");
    printf("Options:\n");
    printf("  --version   Print version information\n");
    printf("  --help      Print this help message\n");
}


int main(int argc, char** argv) {
    process_argc = argc;
    process_argv = argv;

    // Handle command-line flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
    }

    // Ensure a script file is provided
    if (argc < 2) {
        fprintf(stderr, "Error: No script file provided.\n");
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