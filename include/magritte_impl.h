#pragma once

#include "Block.h"
#include "index_cluster.h"
#include "job_conter.h"
#include "lru.h"
#include "magritte_typedefs.h"
#include <atomic>
#include <cstdint>
#include <rw_spin_lock.h>
#include <string>
#include <vector>

struct MagritteMeta {
    char version_major;
    char version_minor;
    char version_patch;
    uint32_t n_blocks;
    uint32_t n_index_blocks;
    uint32_t n_stored_items;
    uint32_t extras;
};

class Magritte {
  public:
    Magritte(std::string filepath, MagritteConfig* config = nullptr);
    Magritte(Magritte&&);
    ~Magritte();
    bool put(MagritteKey key, std::vector<char> value);
    bool get(MagritteKey key, std::vector<char>& value);
    bool remove(MagritteKey key, std::vector<char>& value);
    bool probe(MagritteKey key);
    void shutdown();

  private:
    MagritteMeta meta;
    std::string filepath;
    MagritteConfig config;
    int file;

    std::atomic_bool shutdown_;
    job_couter counter;

    std::vector<Block> blocks;
    std::pair<Block*, int> allocate_block(int expect_size);

    rw_spin_lock allocation_lock;

    IndexCluster indicies;

    // cache recently read data
    LRUCache<MagritteKey, std::vector<char>> r_cache;
    // recently writed data also have cache for fast reading, but in smaller
    // size.
    LRUCache<MagritteKey, std::vector<char>> w_cache;

    bool flush_meta();
};
