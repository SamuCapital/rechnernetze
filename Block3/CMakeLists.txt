cmake_minimum_required(VERSION 3.14)

project(HA_2 C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(client src/utarray.h src/uthash.h src/utlist.h src/utringbuffer.h src/utstack.h src/utstring.h protocol.c hashtable.c protocol.h client.c)

add_executable(server src/utarray.h src/uthash.h src/utlist.h src/utringbuffer.h src/utstack.h src/utstring.h protocol.c hashtable.c protocol.h server.c)