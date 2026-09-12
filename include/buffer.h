#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

/// append raw byte array to dynamic buffer
void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len);

/// consume and remove n bytes from the front of buffer
void buf_consume(std::vector<uint8_t> &buf, size_t n);
