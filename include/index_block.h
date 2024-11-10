#pragma once

// each index blocks store 2^10 indices, containing a series of keys that
// within a lower bound and upper bound, and provide wrapper method for put,
// update and delete, and also a split function that splits the block into two
// index block with splited upper and lower bound if the block is full.

#include "magritte_typedefs.h"
#include "rw_spin_lock.h"
#include <vector>

typedef struct {
    MagritteKey key;
    MagritteIndex index;
} MagritteKeyIndexPair;

const int INDEX_BLOCK_MAX_CAP = 1 << 10;
const int INDEX_BLOCK_DATA_SEG_SIZE =
    INDEX_BLOCK_MAX_CAP * sizeof(MagritteKeyIndexPair);
const int INDEX_BLOCK_SIZE =
    INDEX_BLOCK_DATA_SEG_SIZE + 2 * sizeof(MagritteKey);

class IndexBlock {
  public:
    IndexBlock(MagritteKey lower_bound, MagritteKey upper_bound);
    IndexBlock(MagritteKey lower_bound, MagritteKey upper_bound,
               std::vector<MagritteKeyIndexPair>&& data);
    IndexBlock(IndexBlock&& ib);
    IndexBlock(IndexBlock& ib);

    bool put(MagritteKey key, MagritteIndex value);
    std::pair<bool, IndexBlock> put_or_split_put(MagritteKey key,
                                                   MagritteIndex index);
    bool get(MagritteKey key, MagritteIndex& value);
    bool remove(MagritteKey key, MagritteIndex& index);
    bool fit_in(MagritteKey key);
    bool full();
    bool full_wrt(MagritteKey key);
    bool empty();
    IndexBlock split();
    MagritteKey lower_bound();
    MagritteKey upper_bound();
    int size();
    std::pair<char*, size_t> data();

  private:
    MagritteKey _lower_bound;
    MagritteKey _upper_bound;
    rw_spin_lock lock;
    // std::shared_mutex mutex;
    std::vector<MagritteKeyIndexPair> store;

    void put_lockfree(MagritteKey key, MagritteIndex value);
    IndexBlock split_lockfree();
};
