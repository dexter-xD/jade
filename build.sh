#!/bin/bash

build() {
    mkdir -p build
    cd build
    cmake ..
    make
    ./jade ../scripts/test.js
    cd ..
}

clean() {
    rm -rf build
}

if [ "$1" == "clean" ]; then
    clean
    echo "Build directory cleaned."
else
    build
fi
