#pragma once

#include "BitMap.h"
#include "channel.h"
#include "rw_spin_lock.h"
#include <atomic>
#include <semaphore>
#include <thread>
#include <unordered_map>
#include <vector>

// Constants
const uint32_t BLOCK_SIZE = (1 << 30) + (1 << 12);
const size_t BITMAP_SIZE =
    1 << 17; // number of bytes bitmap occupies, not capacity.
const uint32_t BLOCK_MAX_CAP = 1 << 20;
const size_t BLOCK_BUFFER_SIZE = 512;

typedef uint32_t MagritteInBlockIndex;
typedef std::vector<char> MagritteValue;

class Block {
  public:
    Block(int file, uint32_t offset, BitMap&& bmp);
    Block(int file, uint32_t offset);
    Block(Block&& other);
    ~Block();

    void flush();
    void flush_sync();
    bool vacant() const;
    bool put(MagritteValue data, MagritteInBlockIndex& offset);
    bool update(MagritteValue data, MagritteInBlockIndex offset);
    MagritteValue get(MagritteInBlockIndex inBlockIndex);
    void remove(MagritteInBlockIndex inBlockIndex);
    void shutdown();

  private:
    void flush_worker();
    int64_t get_offset_of(MagritteInBlockIndex i) const;
    bool adjust_vacancy(int diff);

    BitMap bitmap;
    bool bitmapDirty;
    std::atomic<uint32_t> vacancy;
    uint32_t offset;
    std::unordered_map<MagritteInBlockIndex, MagritteValue> pendingChanges;
    rw_spin_lock lock;
    channel<std::binary_semaphore*> flushSignal;
    std::atomic<bool> shutdown_;
    int file_no;
    std::thread flush_thread;
};

