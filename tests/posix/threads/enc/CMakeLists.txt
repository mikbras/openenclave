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
    functional/argv.c
    functional/basename.c
    functional/clocale_mbfuncs.c
    functional/clock_gettime.c
    functional/crypt.c
    functional/dirname.c
    #functional/dlopen.c
    functional/dlopen_dso.c
    functional/env.c
    functional/fcntl.c
    functional/fdopen.c
    functional/fnmatch.c
    functional/fscanf.c
    functional/functional.c
    functional/fwscanf.c
    functional/iconv_open.c
    functional/inet_pton.c
    functional/ipc_msg.c
    functional/ipc_sem.c
    functional/ipc_shm.c
    functional/mbc.c
    functional/memstream.c
    functional/popen.c
    functional/pthread_cancel.c
    functional/pthread_cancel-points.c
    functional/pthread_cond.c
    functional/pthread_mutex.c
    functional/pthread_mutex_pi.c
    functional/pthread_robust.c
    functional/pthread_tsd.c
    functional/qsort.c
    functional/random.c
    functional/search_hsearch.c
    functional/search_insque.c
    functional/search_lsearch.c
    functional/search_tsearch.c
    functional/sem_init.c
    functional/sem_open.c
    functional/setjmp.c
    functional/snprintf.c
    functional/socket.c
    functional/spawn.c
    functional/sscanf.c
    functional/sscanf_long.c
    functional/stat.c
    functional/strftime.c
    functional/string.c
    functional/string_memcpy.c
    functional/string_memmem.c
    functional/string_memset.c
    functional/string_strchr.c
    functional/string_strcspn.c
    functional/string_strstr.c
    functional/strptime.c
    functional/strtod.c
    functional/strtod_long.c
    functional/strtod_simple.c
    functional/strtof.c
    functional/strtol.c
    functional/strtold.c
    functional/swprintf.c
    #functional/tgmath.c
    functional/time.c
    functional/tls_align.c
    functional/tls_align_dlopen.c
    functional/tls_align_dso.c
    functional/tls_init.c
    functional/tls_init_dlopen.c
    functional/tls_init_dso.c
    functional/tls_local_exec.c
    functional/udiv.c
    functional/ungetc.c
    functional/utime.c
    functional/vfork.c
    functional/wcsstr.c
    functional/wcstol.c
    stubs.c
    start.S
    test.c
    ${CMAKE_CURRENT_BINARY_DIR}/posix_t.c)

target_include_directories(posix_enc PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/functional
    /opt/musl/include)

#target_link_libraries(posix_enc -L/opt/musl/lib -lpthread -lc oecore)
target_link_libraries(posix_enc /opt/musl/lib/libc.a oecore)
