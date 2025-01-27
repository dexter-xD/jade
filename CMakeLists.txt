cmake_minimum_required(VERSION 3.10)
project(js_runtime)

set(CMAKE_C_STANDARD 11)

# Find JavaScriptCore
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.0)

# Find libuv
pkg_check_modules(LIBUV REQUIRED libuv)

# Include directories
include_directories(${WEBKIT_INCLUDE_DIRS} ${LIBUV_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)

# Add executable
add_executable(js_runtime
  src/jsc_engine.c
  src/uv_event_loop.c
  src/system_apis.c
  src/main.c
)

# Link libraries
target_link_libraries(js_runtime
  ${WEBKIT_LIBRARIES}
  ${LIBUV_LIBRARIES}
)