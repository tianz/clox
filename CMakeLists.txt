cmake_minimum_required(VERSION 3.0.0)
project(clox)

add_executable(clox
    clox/main.c
    clox/chunk.c
    clox/memory.c
)
