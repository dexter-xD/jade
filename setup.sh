#!/bin/bash

install_linux_deps() {
    echo "Installing dependencies for Linux..."

    sudo apt update
    sudo apt install -y cmake pkg-config libwebkit2gtk-4.0-dev libuv1-dev

    echo "Linux dependencies installed."
}

install_mac_deps() {
    echo "Installing dependencies for macOS..."

    if ! command -v brew &>/dev/null; then
        echo "Homebrew not found. Installing..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi

    brew install cmake pkg-config webkit2gtk libuv

    echo "macOS dependencies installed."
}

configure_for_mac() {
    echo "Configuring CMakeLists.txt for macOS..."

    echo -e "\n# macOS specific configuration" >> CMakeLists.txt
    echo 'if(APPLE)' >> CMakeLists.txt
    echo '    target_link_libraries(jade' >> CMakeLists.txt
    echo '        ${WEBKIT_LIBRARIES}' >> CMakeLists.txt
    echo '        ${LIBUV_LIBRARIES}' >> CMakeLists.txt
    echo '        "-framework JavaScriptCore"' >> CMakeLists.txt
    echo 'else()' >> CMakeLists.txt
    echo '    target_link_libraries(jade' >> CMakeLists.txt
    echo '        ${WEBKIT_LIBRARIES}' >> CMakeLists.txt
    echo '        ${LIBUV_LIBRARIES}' >> CMakeLists.txt
    echo '    )' >> CMakeLists.txt
    echo 'endif()' >> CMakeLists.txt

    echo "CMakeLists.txt configured for macOS."
}

configure_for_linux() {
    echo "Configuring CMakeLists.txt for Linux..."

    echo -e "\n# Linux specific configuration" >> CMakeLists.txt
    echo 'if(NOT APPLE)' >> CMakeLists.txt
    echo '    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.0)' >> CMakeLists.txt
    echo 'endif()' >> CMakeLists.txt

    echo "CMakeLists.txt configured for Linux."
}


if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    install_linux_deps
    configure_for_linux
elif [[ "$OSTYPE" == "darwin"* ]]; then
    install_mac_deps
    configure_for_mac
else
    echo "Unsupported operating system: $OSTYPE"
    exit 1
fi

echo "Setup complete! You can now build the project using ./build.sh."
