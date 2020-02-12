# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

set (EDL_FILE ../posix.edl)

add_custom_command(
    OUTPUT posix_t.h posix_t.c
    DEPENDS ${EDL_FILE} edger8r
    COMMAND edger8r --trusted ${EDL_FILE} --search-path ${CMAKE_CURRENT_SOURCE_DIR})

add_enclave(TARGET posix_enc UUID 71b0822f-42a3-4543-a97c-ca491f76b82c SOURCES enc.c ${CMAKE_CURRENT_BINARY_DIR}/posix_t.c)

target_include_directories(posix_enc PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(posix_enc oelibc)
