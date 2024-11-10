#pragma once

#include "Block.h"
#include <cstdint>

typedef int32_t MagritteKey;
typedef uint32_t MagritteIndex;

typedef struct {
    uint32_t n_read_cache;
    uint32_t n_write_buffer_per_block;
    uint32_t millisec_flush_timeout;
} MagritteConfig;

inline std::pair<uint16_t, MagritteInBlockIndex> get_block_index(MagritteIndex key) {
    return std::make_pair(key >> 20, key & 0x000FFFFF);
}

