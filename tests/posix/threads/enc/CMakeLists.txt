# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

set (EDL_FILE ../posix.edl)

add_custom_command(
    OUTPUT posix_t.h posix_t.c
    DEPENDS ${EDL_FILE} edger8r
    COMMAND edger8r
        --trusted ${EDL_FILE}
        --search-path ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(posix_enc
    enc.c
    pthread_cond.c
    pthread_mutex.c
    pthread_tsd.c
    sem_init_main.c
    stubs.c
    start.S
    test.c
    ${CMAKE_CURRENT_BINARY_DIR}/posix_t.c)

target_include_directories(posix_enc PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    /opt/musl/include)

#target_link_libraries(posix_enc -L/opt/musl/lib -lpthread -lc oecore)
target_link_libraries(posix_enc /opt/musl/lib/libc.a oecore)
