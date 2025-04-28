#!/bin/bash

RUNTIME="./build/jade"
RESULTS_DIR="scripts/results"

if [ ! -f "$RUNTIME" ]; then
    echo "Error: Runtime executable not found at $RUNTIME"
    echo "Please build the runtime first by running:"
    echo "  cd build && cmake .. && make"
    exit 1
fi

if [ "$1" == "--clean" ]; then
    echo "Cleaning saved test results..."
    rm -rf "$RESULTS_DIR"
    echo "All test results have been deleted."
    exit 0
fi

mkdir -p "$RESULTS_DIR"

strip_ansi() {
    sed 's/\x1B\[[0-9;]*[mK]//g'
}


run_test() {
    local test_name="$1"
    local test_script="$2"
    local output_file="$RESULTS_DIR/${test_name// /_}.txt"

    if [ "$MODE" == "save" ]; then
        echo "Saving test result: $test_name -> $output_file"
        echo "---------------------------------" > "$output_file"
        echo "Test: $test_name" >> "$output_file"
        echo "---------------------------------" >> "$output_file"

        "$RUNTIME" "$test_script" 2>&1 | strip_ansi >> "$output_file"

        if [ $? -eq 0 ]; then
            echo "TEST PASSED: $test_name" >> "$output_file"
        else
            echo "TEST FAILED: $test_name" >> "$output_file"
        fi
        echo "---------------------------------" >> "$output_file"
    else
        echo "Running test: $test_name"
        echo "---------------------------------"
        "$RUNTIME" "$test_script"
        if [ $? -eq 0 ]; then
            echo "TEST PASSED: $test_name"
        else
            echo "TEST FAILED: $test_name"
        fi
        echo "---------------------------------"
        echo ""
    fi
}


if [ "$1" == "--save" ]; then
    MODE="save"
else
    MODE="show"
fi


run_test "Console API" "scripts/tests/console.test.js"
run_test "Timers API" "scripts/tests/timers.test.js"
run_test "Process API" "scripts/tests/process.test.js"
run_test "Runtime Info" "scripts/tests/runtime.test.js"

if [ "$MODE" == "save" ]; then
    echo "Saving process.exit test result..."
    "$RUNTIME" "scripts/tests/process.test.js" arg1 arg2 2>&1 | strip_ansi > "$RESULTS_DIR/process_exit_test.txt"
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 42 ]; then
        echo "PROCESS.EXIT TEST: PASSED" >> "$RESULTS_DIR/process_exit_test.txt"
    else
        echo "PROCESS.EXIT TEST: FAILED (Got $EXIT_CODE)" >> "$RESULTS_DIR/process_exit_test.txt"
    fi
else
    echo "Testing process.exit (should exit with code 42)"
    "$RUNTIME" "scripts/tests/process.test.js" arg1 arg2
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 42 ]; then
        echo "PROCESS.EXIT TEST: PASSED"
    else
        echo "PROCESS.EXIT TEST: FAILED (Got $EXIT_CODE)"
    fi
    echo "---------------------------------"
fi
