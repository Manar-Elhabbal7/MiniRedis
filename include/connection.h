#pragma once

#include <vector>
#include <cstdint>

/// per-connection state tracked by the event loop
struct Conn {
    int fd = -1;
    /// application readiness intent
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    /// buffered non-blocking input and output data
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};
