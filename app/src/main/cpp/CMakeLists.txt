cmake_minimum_required(VERSION 3.22.1)
project(fmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(fmark SHARED
        jni_bridge.cpp
        gl_engine.cpp
)

target_compile_options(fmark PRIVATE -Wall -Wextra -O2)
target_include_directories(fmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)

target_link_libraries(fmark ${log-lib} ${android-lib} ${egl-lib} ${gles-lib})