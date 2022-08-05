cmake_minimum_required(VERSION 3.0.0)
project(clox)

add_executable(clox
    clox/chunk.c
    clox/debug.c
    clox/main.c
    clox/memory.c
    clox/value.c
    clox/vm.c
)
