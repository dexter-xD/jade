#!/bin/bash

build() {
    mkdir -p build
    cd build
    cmake ..
    make
    ./jade ../scripts/test.js
    cd ..
}

run() {
    if [ ! -d "build" ]; then
        echo "Build directory does not exist. Building project first."
        build
    fi

    cd build
    ./jade ../scripts/test.js
    cd ..
}

clean() {
    rm -rf build
}

if [ "$1" == "clean" ]; then
    clean
    echo "Build directory cleaned."
elif [ "$1" == "run" ]; then
    run
else
    build
fi
