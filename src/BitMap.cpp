#include "BitMap.h"
#include <bitset>
#include <cstdint>
#include <mutex>
#include <stdexcept>

BitMap::BitMap(int size) : lastVacant(0) {
    int bytes = size / 8;
    if (size % 8 != 0) {
        bytes += 1;
    }
    store.resize(bytes);
}

// Copy constractor
BitMap::BitMap(BitMap& bmp) {
    this->store = bmp.store;
    this->lastVacant = bmp.lastVacant;
}

// Move constracotr
BitMap::BitMap(BitMap&& other) {
    this->store = std::move(other.store);
    this->lastVacant = other.lastVacant;
}

uint32_t BitMap::count_vacant() {
    auto count = 0;
    for (int i = 0; i < store.size(); ++i) {
        std::bitset<8> bits(store[i]);
        count += bits.count();
    }
    return store.size() * 8 - count;
}

// Copy assignment
BitMap& BitMap::operator=(BitMap& bmp) {
    this->store = bmp.store;
    this->lastVacant = bmp.lastVacant;
    return *this;
}

BitMap BitMap::LoadExisting(std::vector<unsigned char>&& b) {
    if (b.empty()) {
        throw std::invalid_argument("bitmap: invalid byte length");
    }

    BitMap bmp(0);
    bmp.store = std::move(b);
    return bmp;
}

void BitMap::Set(int i, bool v) {
    auto byteIndex = i / 8;
    auto bitIndex = i % 8;
    auto mask = bitMask32(i);

    do {
        auto oldValue = __atomic_load_n(&store[byteIndex], __ATOMIC_SEQ_CST);
        auto newValue = oldValue;
        if (v) {
            newValue |= mask;
        } else {
            newValue &= ~mask;
        }

        auto ok = __atomic_compare_exchange_n(&store[byteIndex], &oldValue,
                                              newValue, false, __ATOMIC_SEQ_CST,
                                              __ATOMIC_SEQ_CST);
        if (ok) {
            std::scoped_lock<std::mutex> lock(vacantMutex);
            if (i < lastVacant && !v) {
                lastVacant = i;
            }
            return;
        }

    } while (true);
}

// Get the value of the bit at index `i`
bool BitMap::Get(int i) const {
    int byteIndex = i / 8;
    int bitIndex = i % 8;
    unsigned char mask = 1 << bitIndex;

    auto bytePtr = &store[byteIndex];
    auto byteValue = __atomic_load_n(bytePtr, __ATOMIC_SEQ_CST);

    return (byteValue & mask) != 0;
}

// Find a vacant bit and set it
int BitMap::FindVacantAndSet() {
    std::unique_lock<std::mutex> lock(vacantMutex);

    int nextVacant = -1;
    for (size_t i = lastVacant / 8; i < store.size(); ++i) {
        if (store[i] != static_cast<unsigned char>(0xFF)) {
            unsigned char byte = ~store[i];
            unsigned char complement = -byte;
            unsigned char bitwiseIndex = fastLog2(byte & complement);

            nextVacant = i * 8 + bitwiseIndex;
            break;
        }
    }

    lastVacant = nextVacant;

    if (nextVacant != -1) {
        int byteIndex = nextVacant / 8;
        int bitIndex = nextVacant % 8;
        store[byteIndex] |= static_cast<unsigned char>(1 << bitIndex);
    }

    return nextVacant;
}

// Get the underlying bytes of the bitmap
const std::vector<unsigned char> BitMap::GetUnderlayBytes() {
    return this->store;
}

uint32_t BitMap::bitMask32(int i) {
    int bitOff = i % 8;
    int bytOff = (i % 32) / 8;
    return static_cast<uint32_t>(1 << (bytOff * 8 + bitOff));
}

// Fast log base 2 function for bytes
unsigned char BitMap::fastLog2(unsigned char b) {
    unsigned char i = 0;
    while (b != 1 && i < 8) {
        b >>= 1;
        ++i;
    }
    return i;
}
