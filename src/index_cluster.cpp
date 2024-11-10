#include "index_cluster.h"
#include "index_block.h"
#include "magritte_typedefs.h"
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

// if n_blocks_in_file == 0, then a index block with INT_MIN and INT_MAX will be
// created.
IndexCluster::IndexCluster(int file, uint32_t offset, uint32_t n_blocks_in_file)
    : file(file), offset(offset), dirty_mark(n_blocks_in_file) {
    for (auto i = 0; i < n_blocks_in_file; i++) {
        MagritteKey upperbound, lowerbound;

        auto n_bytes =
            pread(file, &lowerbound, 4, offset + i * INDEX_BLOCK_SIZE);
        if (n_bytes != 4) {
            std::string message =
                "index-cluster: failed load lowerbound of block " +
                std::to_string(i) + " to from file: ";
            message += std::strerror(errno);
            throw std::runtime_error(message);
        }

        n_bytes =
            pread(file, &upperbound, 4, offset + i * INDEX_BLOCK_SIZE + 4);
        if (n_bytes != 4) {
            std::string message =
                "index-cluster: failed load upperbound of block " +
                std::to_string(i) + " to from file: ";
            message += std::strerror(errno);
            throw std::runtime_error(message);
        }

        auto indicies = std::vector<MagritteKeyIndexPair>(INDEX_BLOCK_MAX_CAP);
        n_bytes = pread(file, indicies.data(), INDEX_BLOCK_DATA_SEG_SIZE,
                        offset + i * INDEX_BLOCK_SIZE + 8);
        if (n_bytes != INDEX_BLOCK_DATA_SEG_SIZE) {
            std::string message =
                "index-cluster: failed load data segment of block " +
                std::to_string(i) + " to from file: ";
            message += std::strerror(errno);
            throw std::runtime_error(message);
        }

        index_blocks.emplace_back(lowerbound, upperbound, std::move(indicies));
    }

    if (n_blocks_in_file == 0) {
        this->index_blocks.emplace_back(INT_MIN, INT_MAX);
        this->dirty_mark.emplace_back(true);
    }
}

IndexCluster::~IndexCluster() { this->flush(FlushReason::Manually); }

IndexCluster& IndexCluster::operator=(IndexCluster&& other) {
    lock.lock();

    this->index_blocks = std::move(other.index_blocks);
    this->offset = other.offset;
    this->file = other.file;

    lock.unlock();
    return *this;
}

bool IndexCluster::set_offset(uint32_t offset) {
    this->offset = offset;
    return this->flush(FlushReason::OffsetChange);
}

bool IndexCluster::get(MagritteKey key, MagritteIndex& index) {
    lock.lock_shared();

    for (auto& block : this->index_blocks) {
        if (block.fit_in(key)) {
            lock.unlock_shared();
            return block.get(key, index);
        }
    }

    lock.unlock_shared();
    return false;
}

bool IndexCluster::put(MagritteKey key, MagritteIndex index) {
    lock.lock();

    size_t i = 0;
    for (i = 0; i < this->index_blocks.size(); i++) {
        if (this->index_blocks[i].fit_in(key))
            break;
    }

    auto& block = this->index_blocks[i];
    auto [splited, higher] = block.put_or_split_put(key, index);

    this->dirty_mark[i] = true;
    if (splited) {
        this->index_blocks.emplace_back(higher);
        this->dirty_mark.emplace_back(true);
    }

    lock.unlock();
    return true;
}

bool IndexCluster::remove(MagritteKey key, MagritteIndex& index) {
    auto is_highest_key = key == INT32_MAX;

    size_t i = 0;
    for (auto& block : this->index_blocks) {
        if (block.fit_in(key)) {
            auto success = block.remove(key, index);
            this->dirty_mark[i] = true;
            return success;
        }
    }

    return false;
}

bool IndexCluster::flush(FlushReason reason) {
    lock.lock();

    for (auto i = 0; i < this->index_blocks.size(); i++) {
        if (reason != FlushReason::OffsetChange && !dirty_mark[i])
            continue;

        auto& block = this->index_blocks[i];
        auto upperbound = block.upper_bound();
        auto lowerbound = block.lower_bound();
        auto data = block.data();
        pwrite(this->file, &lowerbound, 4, this->offset + i * INDEX_BLOCK_SIZE);
        pwrite(this->file, &upperbound, 4,
               this->offset + i * INDEX_BLOCK_SIZE + sizeof(MagritteKey));
        pwrite(this->file, data.first, data.second,
               this->offset + i * INDEX_BLOCK_SIZE + 2 * sizeof(MagritteKey));
    }

    lock.unlock();
    return true;
}

size_t IndexCluster::size() { return this->index_blocks.size(); }
