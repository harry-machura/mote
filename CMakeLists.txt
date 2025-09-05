cmake_minimum_required(VERSION 3.16)
project(mote_host C)

set(SOURCES
    src/vm.c
    src/hal_stub.c
    src/main_host.c
)

add_executable(mote_host ${SOURCES})

# ---- Mote High-Level Compiler (C) ----
add_executable(motec tools/motec.c tools/motec_additions.c)