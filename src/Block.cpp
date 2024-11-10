#include "Block.h"
#include "BitMap.h"

#include <cstdlib>
#include <fcntl.h>
#include <semaphore>
#include <unistd.h>
#include <unordered_map>

Block::Block(int file, uint32_t offset, BitMap&& bmp)
    : file_no(file), offset(offset), shutdown_(false), flushSignal(1),
      bitmap(std::move(bmp)), bitmapDirty(false) {
    vacancy = bitmap.count_vacant();
    flush_thread = std::thread(&Block::flush_worker, this);
}

Block::Block(int file, uint32_t offset)
    : file_no(file), offset(offset), shutdown_(false), flushSignal(1),
      bitmap(BLOCK_MAX_CAP), bitmapDirty(false), vacancy(BLOCK_MAX_CAP) {
    flush_thread = std::thread(&Block::flush_worker, this);
}

Block::Block(Block&& other)
    : file_no(other.file_no), offset(other.offset), shutdown_(false),
      flushSignal(1), bitmap(std::move(other.bitmap)) {
    vacancy = other.vacancy.load();
    other.shutdown();

    flush_thread = std::thread(&Block::flush_worker, this);
}

Block::~Block() { shutdown(); }

int64_t Block::get_offset_of(MagritteInBlockIndex i) const {
    return offset + i * 1024 + BITMAP_SIZE;
}

bool Block::adjust_vacancy(int diff) {
    if (diff > 0) {
        vacancy.fetch_add(diff);
    } else {
        vacancy.fetch_sub(abs(diff));
    }
    return true;
}

bool Block::vacant() const { return vacancy.load() != 0; }

bool Block::put(MagritteValue data, MagritteInBlockIndex& offset) {
    if (pendingChanges.size() >= BLOCK_BUFFER_SIZE) {
        flush_sync();
    }

    if (vacancy.load() == 0 || !adjust_vacancy(-1)) {
        return false;
    }

    offset = bitmap.FindVacantAndSet();
    if (offset == -1) {
        return false;
    }

    bitmapDirty = true;

    lock.lock();
    pendingChanges[offset] = std::move(data);
    lock.unlock();
    return true;
}

bool Block::update(MagritteValue data, MagritteInBlockIndex offset) {
    if (pendingChanges.size() >= BLOCK_BUFFER_SIZE) {
        flush_sync();
    }

    lock.lock();
    pendingChanges[offset] = std::move(data);
    lock.unlock();

    return true;
}

MagritteValue Block::get(MagritteInBlockIndex inBlockIndex) {
    lock.lock_shared();

    auto it = pendingChanges.find(inBlockIndex);
    if (it != pendingChanges.end()) {
        lock.unlock_shared();
        return it->second;
    }
    MagritteValue buffer(1024);
    auto n_bytes = pread(this->file_no, buffer.data(), 1024,
                         this->get_offset_of(inBlockIndex));
    if (n_bytes < 0) {
        lock.unlock_shared();
        throw std::runtime_error("Failed to read block");
    }

    lock.unlock_shared();
    return buffer;
}

void Block::remove(MagritteInBlockIndex inBlockIndex) {
    lock.lock_shared();

    pendingChanges.erase(inBlockIndex);
    bitmap.Set(inBlockIndex, false);
    bitmapDirty = true;
    adjust_vacancy(1);

    lock.lock_shared();
}

void Block::flush() {
    this->flushSignal << static_cast<std::binary_semaphore*>(nullptr);
}

void Block::flush_sync() {
    std::binary_semaphore wait(0);
    this->flushSignal << &wait;
    wait.acquire();
}

void Block::shutdown() {
    if (flush_thread.joinable()) {
        flush_sync();
        shutdown_ = true;
        flush_thread.join();
    }

    if (this->file_no >= 0) {
        close(this->file_no);
        this->file_no = -1;
    }
}

void Block::flush_worker() {
    while (!shutdown_) {
        auto [semaphore, _] = this->flushSignal.pop_timeout(500);

        if (this->pendingChanges.size() == 0) {
            if (semaphore)
                semaphore->release();
            continue;
        }

        lock.lock();
        std::unordered_map<MagritteInBlockIndex, MagritteValue> changes;
        changes.swap(pendingChanges);

        for (const auto& entry : changes) {
            pwrite(file_no, entry.second.data(), entry.second.size(),
                   this->get_offset_of(entry.first));
        }
        lock.unlock();

        if (bitmapDirty) {
            pwrite(file_no, bitmap.GetUnderlayBytes().data(), BITMAP_SIZE,
                   offset);
            bitmapDirty = false;
        }

        if (semaphore) {
            semaphore->release();
        }
    }
}
