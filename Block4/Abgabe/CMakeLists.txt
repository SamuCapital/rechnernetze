cmake_minimum_required(VERSION 3.14)

project(HA_4 C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(peer src/utarray.h src/uthash.h src/utlist.h src/utringbuffer.h src/utstack.h src/utstring.h protocol.c  protocol.h peer.c)