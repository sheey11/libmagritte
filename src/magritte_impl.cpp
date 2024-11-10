#include "magritte_impl.h"
#include "Block.h"
#include "job_conter.h"
#include "magritte_typedefs.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

Magritte::Magritte(std::string filepath, MagritteConfig* config)
    : filepath(filepath), indicies(0, 0, 0), r_cache(1024 * 3),
      w_cache(1024 * 1) {
    if (filepath.empty()) {
        throw std::runtime_error("expect filepath, got empty string");
    }

    // probe file exists and record its size.
    uint32_t fsize;
    if (access(filepath.c_str(), F_OK) == 0) {
        struct stat st;
        if (stat(filepath.c_str(), &st) != 0) {
            std::string message = "Failed to get file size of " + filepath;
            message += std::strerror(errno);
            throw std::runtime_error(message);
        }
        fsize = st.st_size;
    } else {
        fsize = 0;
    }

    // open or create file.
    this->file = open(filepath.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (this->file < 0) {
        std::string message = "Failed to open file at " + filepath + ": ";
        message += std::strerror(errno);
        throw std::runtime_error(message);
    }

    // read metadata or create new one.
    if (fsize > sizeof(MagritteMeta)) {
        if (read(this->file, &this->meta, sizeof(MagritteMeta)) !=
            sizeof(MagritteMeta)) {
            std::string message = "Failed to read metadata from " + filepath;
            message += std::strerror(errno);
            throw std::runtime_error(message);
        }
    } else {
        this->meta = {
            .version_major = 0,
            .version_minor = 1,
            .version_patch = 0,
            .n_blocks = 0,
            .n_stored_items = 0,
            .extras = 0,
        };
        if (write(this->file, &this->meta, sizeof(MagritteMeta)) !=
            sizeof(MagritteMeta)) {
            throw std::runtime_error("Failed to write meta data to " +
                                     filepath);
        }
    }

    // prepare n_blocks recorded in metadata, load bitmap accordingly.
    for (uint32_t i = 0; i < this->meta.n_blocks; ++i) {
        auto offset = sizeof(MagritteMeta) + i * BLOCK_SIZE;
        auto file_for_block = open(filepath.c_str(), O_RDWR, S_IRUSR | S_IWUSR);

        std::vector<unsigned char> buffer(BITMAP_SIZE);
        auto read_size =
            pread(file_for_block, buffer.data(), BITMAP_SIZE, offset);
        if (read_size != BITMAP_SIZE)
            throw std::runtime_error("Failed to read bitmap from file");

        auto bmp = BitMap::LoadExisting(std::move(buffer));
        this->blocks.emplace_back(file_for_block, offset, std::move(bmp));
    }

    // create the first block
    if (this->meta.n_blocks == 0) {
        auto file = open(this->filepath.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
        auto offset = sizeof(MagritteMeta) + BLOCK_SIZE;
        this->indicies.set_offset(sizeof(MagritteMeta) +
                                  (this->blocks.size() + 1) * BLOCK_SIZE);
        auto& block = this->blocks.emplace_back(file, offset);

        this->meta.n_blocks = 1;
    }

    // load indicies
    this->indicies = std::move(IndexCluster(
        this->file, sizeof(MagritteMeta) + this->meta.n_blocks * BLOCK_SIZE,
        this->meta.n_index_blocks));

    // record config, for now it doesn't take any effect :)
    if (config) {
        this->config = *config;
    } else {
        this->config = MagritteConfig{
            .n_read_cache = 1024,
            .n_write_buffer_per_block = 32,
            .millisec_flush_timeout = 500,
        };
    }
}

Magritte::Magritte(Magritte&& other)
    : counter(), w_cache(1024), r_cache(1024 * 3), indicies(0, 0, 0) {
    other.shutdown_ = true;
    other.counter.wait_zero();

    file = other.file;
    config = std::move(other.config);
    filepath = std::move(other.filepath);
    meta = std::move(other.meta);
    blocks = std::move(other.blocks);
    w_cache = std::move(other.w_cache);
    r_cache = std::move(other.r_cache);
    indicies = std::move(other.indicies);
    shutdown_ = false;
}

bool Magritte::get(MagritteKey key, std::vector<char>& value) {
    if (this->shutdown_)
        return false;
    scoped_couter cntr(this->counter);

    if (this->r_cache.get(key, value))
        return true;
    if (this->w_cache.get(key, value))
        return true;

    MagritteIndex index = 0;
    if (!this->indicies.get(key, index)) {
        std::cerr << "get(" << key << ") failed: index record not found"
                  << std::endl;
        return false;
    }

    auto [block_index, in_block_index] = get_block_index(index);

    if (block_index > this->blocks.size() - 1) {
        std::cerr << "get() failed: block not found" << std::endl;
        return false;
    }

    auto block = &this->blocks[block_index];
    value = block->get(in_block_index);

    this->r_cache.put(key, value);

    return true;
}

bool Magritte::probe(MagritteKey key) {
    if (this->shutdown_)
        return false;
    scoped_couter cntr(this->counter);

    MagritteIndex index;
    return this->indicies.get(key, index);
}

bool Magritte::flush_meta() {
    scoped_couter cntr(this->counter);

    return pwrite(this->file, &this->meta, sizeof(MagritteMeta), 0) ==
           sizeof(MagritteMeta);
}

std::pair<Block*, int> Magritte::allocate_block(int expect_size) {
    scoped_couter cntr(this->counter);

    allocation_lock.lock();

    if (expect_size < blocks.size()) {
        // already allocated at another thread
        auto block = &(*this->blocks.end());
        auto i = this->blocks.size() - 1;
        allocation_lock.unlock();
        return std::make_pair(block, i);
    }

    auto file = open(this->filepath.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    auto offset = sizeof(MagritteMeta) + this->blocks.size() * BLOCK_SIZE;
    this->indicies.set_offset(sizeof(MagritteMeta) +
                              (this->blocks.size() + 1) * BLOCK_SIZE);
    auto block = &this->blocks.emplace_back(file, offset);
    auto i = this->blocks.size() - 1;

    this->meta.n_blocks++;
    this->flush_meta();

    block->flush_sync();

    allocation_lock.unlock();
    return std::make_pair(block, i);
}

bool Magritte::put(MagritteKey key, std::vector<char> value) {
    if (this->shutdown_)
        return false;
    scoped_couter cntr(this->counter);

    MagritteIndex index = 0;
    Block* block = nullptr;

    // update exsiting value
    auto update = false;
    update = this->indicies.get(key, index);

    if (update) {
        auto [block_index, in_block_index] = get_block_index(index);
        block = &this->blocks[block_index];
        this->r_cache.remove(key);
        this->w_cache.put(key, value);
        auto success = block->update(value, in_block_index);
        if (!success)
            std::cerr << "put(" << key << ") failed: updating exsiting value"
                      << std::endl;
        return success;
    }

    // insert new value
    auto i = 0;
    auto curr_n_blocks = this->blocks.size();
    for (; i < curr_n_blocks; i++) {
        if (blocks[i].vacant()) {
            block = &blocks[i];
            break;
        }
    }

    if (!block) {
        std::cerr << "Allocating new block with i: " << i << std::endl;
        auto allocation_result = this->allocate_block(curr_n_blocks);
        block = allocation_result.first;
        i = allocation_result.second;
    }

    MagritteInBlockIndex in_block_index;
    auto success = block->put(value, in_block_index);

    if (!success) {
        // allocate block and try again
        auto allocation_result = this->allocate_block(curr_n_blocks);
        block = allocation_result.first;
        i = allocation_result.second;

        success = block->put(value, in_block_index);
        if (!success) {
            std::cerr << "put() failed: block put failed" << std::endl;
            return false;
        }
    }

    index = in_block_index + (i << 20);
    success = this->indicies.put(key, index);
    if (!success)
        std::cerr << "put() failed: indicies put failed" << std::endl;
    return success;
}

bool Magritte::remove(MagritteKey key, std::vector<char>& value) {
    if (this->shutdown_)
        return false;
    scoped_couter cntr(this->counter);

    MagritteIndex index;
    if (!this->indicies.remove(key, index))
        return false;

    auto [block_index, in_block_index] = get_block_index(index);

    auto& block = this->blocks[block_index];
    value = block.get(in_block_index);

    this->r_cache.remove(key);
    this->w_cache.remove(key);

    block.remove(in_block_index);

    return true;
}

void Magritte::shutdown() {
    this->shutdown_ = true;
    this->flush_meta();
    this->counter.wait_zero();
    close(this->file);
}

Magritte::~Magritte() { this->shutdown(); }
