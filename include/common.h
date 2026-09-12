#pragma once

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>

/// maximum allowed message payload size (32MB)
const size_t k_max_msg = 32 << 20;

/// maximum allowed arguments in a command
const size_t k_max_args = 200 * 1000;

/// response status codes
enum {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};

/// log a message to standard output
void msg(const char *m);

/// log error message with errno and abort
void die(const char *m);

/// set a file descriptor to non-blocking mode
void fd_set_nonblock(int fd);

/// read exactly n bytes from a blocking socket into buffer
int32_t read_full(int fd, char *buf, size_t n);

/// write exactly n bytes from buffer into a blocking socket
int32_t write_all(int fd, const char *buf, size_t n);
