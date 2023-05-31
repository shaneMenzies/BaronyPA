cmake_minimum_required(VERSION 3.15)

project(BaronyPA
        VERSION "1.0.0"
        DESCRIPTION "Mumble plugin for positional audio in Barony"
        LANGUAGES "C"
        )

add_library(baronyPA
        SHARED
        plugin.c
        )

target_include_directories(baronyPA
        PUBLIC "${CMAKE_SOURCE_DIR}/include/"
        )
