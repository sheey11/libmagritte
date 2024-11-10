#pragma once

#include "index_block.h"
#include "magritte_typedefs.h"
#include "rw_spin_lock.h"
#include <vector>

typedef enum {
    Timeout,
    Manually,
    OffsetChange,
} FlushReason;

class IndexCluster {
  public:
    IndexCluster(int file, uint32_t offset, uint32_t n_blocks);
    ~IndexCluster();

    IndexCluster& operator=(IndexCluster&) = delete;
    IndexCluster& operator=(IndexCluster&&);
    bool get(MagritteKey, MagritteIndex&);
    bool put(MagritteKey, MagritteIndex);
    bool remove(MagritteKey, MagritteIndex&);
    bool flush(FlushReason reason);
    bool set_offset(uint32_t offset);
    size_t size();

  private:
    std::vector<IndexBlock> index_blocks;
    std::vector<bool> dirty_mark;
    uint32_t offset;
    int file;

    rw_spin_lock lock;
};

