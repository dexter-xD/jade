cmake_minimum_required(VERSION 3.10)
project(jade)

set(CMAKE_C_STANDARD 99)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.0)
pkg_check_modules(LIBUV REQUIRED libuv)

include_directories(
    ${WEBKIT_INCLUDE_DIRS}
    ${LIBUV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

add_executable(jade
    src/jsc_engine.c
    src/uv_event_loop.c
    src/system_apis.c
    src/main.c
)

target_link_libraries(jade
    ${WEBKIT_LIBRARIES}
    ${LIBUV_LIBRARIES}
)
