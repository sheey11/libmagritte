#include "index_block.h"
#include "magritte_typedefs.h"
#include <climits>
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>

IndexBlock::IndexBlock(MagritteKey lower_bound, MagritteKey upper_bound)
    : _lower_bound(lower_bound), _upper_bound(upper_bound) {
    if (lower_bound >= upper_bound)
        throw std::invalid_argument(
            "lower_bound should be less than upper_bound");

    this->store.reserve(INDEX_BLOCK_MAX_CAP);
}

IndexBlock::IndexBlock(MagritteKey lower_bound, MagritteKey upper_bound,
                       std::vector<MagritteKeyIndexPair>&& data)
    : _lower_bound(lower_bound), _upper_bound(upper_bound),
      store(std::move(data)) {
    if (lower_bound >= upper_bound)
        throw std::invalid_argument(
            "lower_bound should be less than upper_bound");
}

IndexBlock::IndexBlock(IndexBlock& ib)
    : _lower_bound(ib._lower_bound), _upper_bound(ib._upper_bound) {
    this->store = ib.store;
}

IndexBlock::IndexBlock(IndexBlock&& ib)
    : _lower_bound(ib._lower_bound), _upper_bound(ib._upper_bound) {
    this->store = std::move(ib.store);
}

bool IndexBlock::put(MagritteKey key, MagritteIndex index) {
    if (!this->fit_in(key))
        throw std::invalid_argument("key " + std::to_string(key) +
                                    " is not in the range of this block [" +
                                    std::to_string(this->_lower_bound) + ", " +
                                    std::to_string(this->_upper_bound) + ")");

    lock.lock();

    bool found = false;
    for (auto it = this->store.begin(); it < this->store.end(); it++) {
        if (it->key == key) {
            it->index = index;
            found = true;
            break;
        }
    }

    if (!found && this->store.size() == INDEX_BLOCK_MAX_CAP) {
        lock.unlock();
        return false;
    }

    if (!found)
        this->store.push_back({key, index});

    lock.unlock();
    return true;
}

void IndexBlock::put_lockfree(MagritteKey key, MagritteIndex index) {
    this->store.push_back({key, index});
}

std::pair<bool, IndexBlock>
IndexBlock::put_or_split_put(MagritteKey key, MagritteIndex index) {
    if (!this->fit_in(key))
        throw std::invalid_argument("key " + std::to_string(key) +
                                    " is not in the range of this block [" +
                                    std::to_string(this->_lower_bound) + ", " +
                                    std::to_string(this->_upper_bound) + ")");

    lock.lock();

    bool found = false;
    for (auto it = this->store.begin(); it < this->store.end(); it++) {
        if (it->key == key) {
            it->index = index;
            found = true;
            break;
        }
    }

    if (!found && this->store.size() == INDEX_BLOCK_MAX_CAP) {
        auto higher = this->split_lockfree();
        if (key >= higher.lower_bound())
            higher.put_lockfree(key, index);
        else
            this->put_lockfree(key, index);

        lock.unlock();
        return std::make_pair(true, higher);
    } else if (!found) {
        this->store.push_back({key, index});
    }

    lock.unlock();
    return std::make_pair(false, IndexBlock(0, 1));
}

bool IndexBlock::get(MagritteKey key, MagritteIndex& index) {
    if (!this->fit_in(key))
        throw std::invalid_argument("key " + std::to_string(key) +
                                    " is not in the range of this block [" +
                                    std::to_string(this->_lower_bound) + ", " +
                                    std::to_string(this->_upper_bound) + ")");

    this->lock.lock_shared();

    for (auto pair : this->store) {
        if (pair.key == key) {
            index = pair.index;

            lock.unlock_shared();
            return true;
        }
    }

    lock.unlock_shared();
    return false;
}

bool IndexBlock::remove(MagritteKey key, MagritteIndex& index) {
    if (!this->fit_in(key))
        throw std::invalid_argument("key " + std::to_string(key) +
                                    " is not in the range of this block [" +
                                    std::to_string(this->_lower_bound) + ", " +
                                    std::to_string(this->_upper_bound) + ")");

    lock.lock();

    for (auto it = this->store.begin(); it < this->store.end(); it++) {
        if (it->key == key) {
            index = it->index;
            this->store.erase(it);

            lock.unlock();
            return true;
        }
    }

    lock.unlock();
    return false;
}

bool IndexBlock::fit_in(MagritteKey key) {
    if (this->_upper_bound == INT_MAX && key == INT_MAX)
        return true;
    if (this->_lower_bound <= key && key < this->_upper_bound)
        return true;
    return false;
}

bool IndexBlock::full() { return this->store.size() == INDEX_BLOCK_MAX_CAP; }

bool IndexBlock::full_wrt(MagritteKey key) {
    auto vacant = this->store.size() != INDEX_BLOCK_MAX_CAP;
    if (vacant)
        return false;

    lock.lock_shared();

    for (auto& pair : this->store) {
        if (pair.key == key) {
            lock.unlock_shared();
            return false;
        }
    }

    lock.unlock_shared();
    return true;
}
bool IndexBlock::empty() { return this->store.size() == 0; }

IndexBlock IndexBlock::split() {
    lock.lock();
    auto block = this->split_lockfree();
    lock.unlock();

    return block;
}

IndexBlock IndexBlock::split_lockfree() {
    int64_t correction = this->_upper_bound == INT_MAX ? 1 : 0;
    MagritteKey mid = ((int64_t)(this->_lower_bound) +
                       (int64_t)(this->_upper_bound) + correction) /
                      2;

    auto previous_upper_bound = this->_upper_bound;
    this->_upper_bound = mid;

    if (this->empty()) {
        return IndexBlock(mid, previous_upper_bound);
    }

    auto indicies_in_higher_range = std::vector<MagritteKeyIndexPair>();
    indicies_in_higher_range.reserve(INDEX_BLOCK_MAX_CAP);

    for (auto it = this->store.begin(); it != this->store.end();) {
        if (it->key >= mid) {
            indicies_in_higher_range.push_back({it->key, it->index});
            it = this->store.erase(it);
        } else {
            ++it;
        }
    }

    return IndexBlock(mid, previous_upper_bound,
                      std::move(indicies_in_higher_range));
}

MagritteKey IndexBlock::lower_bound() { return this->_lower_bound; }
MagritteKey IndexBlock::upper_bound() { return this->_upper_bound; }

int IndexBlock::size() { return this->store.size(); }
std::pair<char*, size_t> IndexBlock::data() {
    auto data = reinterpret_cast<char*>(this->store.data());
    return std::make_pair(data,
                          this->store.size() * sizeof(MagritteKeyIndexPair));
}
