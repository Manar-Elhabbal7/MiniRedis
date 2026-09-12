#include "common.h"

using namespace std;

/// log a message to standard output
void msg(const char *m) {
    cout << m << endl;
}

/// log error message with errno and abort
void die(const char *m) {
    int err = errno;
    cerr << "[" << err << "] " << m << ": " << strerror(err) << endl;
    abort();
}

/// set a file descriptor to non-blocking mode
void fd_set_nonblock(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl F_GETFL");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl F_SETFL");
    }
}

/// read exactly n bytes from a blocking socket into buffer
int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv < 0) {
            if (errno == EINTR) {
                continue; /// retry if interrupted by signal
            }
            return -1; /// error
        }
        if (rv == 0) {
            return -1; /// unexpected EOF
        }
        assert(static_cast<size_t>(rv) <= n);
        n -= static_cast<size_t>(rv);
        buf += rv;
    }
    return 0;
}

/// write exactly n bytes from buffer into a blocking socket
int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv < 0) {
            if (errno == EINTR) {
                continue; /// retry if interrupted by signal
            }
            return -1; /// error
        }
        assert(static_cast<size_t>(rv) <= n);
        n -= static_cast<size_t>(rv);
        buf += rv;
    }
    return 0;
}
