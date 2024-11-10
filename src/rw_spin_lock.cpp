#include "rw_spin_lock.h"
#include <cstdint>
#include <thread>

void rw_spin_lock::lock() {
    uint_fast32_t count = 0;
    while (!try_lock()) {
        if (++count > 2000)
            std::this_thread::yield();
    }
}

void rw_spin_lock::unlock() {
    static_assert(READER > WRITER + UPGRADED, "wrong bits!");
    bits_.fetch_and(~(WRITER | UPGRADED), std::memory_order_release);
}

void rw_spin_lock::lock_shared() {
    uint_fast32_t count = 0;
    while (!try_lock_shared()) {
        if (++count > 1000) {
            std::this_thread::yield();
        }
    }
}

void rw_spin_lock::unlock_shared() {
    bits_.fetch_add(-READER, std::memory_order_release);
}

void rw_spin_lock::unlock_and_lock_shared() {
    bits_.fetch_add(READER, std::memory_order_acquire);
    unlock();
}

void rw_spin_lock::lock_upgrade() {
    uint_fast32_t count = 0;
    while (!try_lock_upgrade()) {
        if (++count > 1000) {
            std::this_thread::yield();
        }
    }
}

void rw_spin_lock::unlock_upgrade() {
    bits_.fetch_add(-UPGRADED, std::memory_order_acq_rel);
}

void rw_spin_lock::unlock_upgrade_and_lock() {
    int64_t count = 0;
    while (!try_unlock_upgrade_and_lock()) {
        if (++count > 1000) {
            std::this_thread::yield();
        }
    }
}

void rw_spin_lock::unlock_upgrade_and_lock_shared() {
    bits_.fetch_add(READER - UPGRADED, std::memory_order_acq_rel);
}

void rw_spin_lock::unlock_and_lock_upgrade() {
    bits_.fetch_or(UPGRADED, std::memory_order_acquire);
    bits_.fetch_add(-WRITER, std::memory_order_release);
}

bool rw_spin_lock::try_lock() {
    int32_t expect = 0;
    return bits_.compare_exchange_strong(expect, WRITER,
                                         std::memory_order_acq_rel);
}

bool rw_spin_lock::try_lock_shared() {
    int32_t value = bits_.fetch_add(READER, std::memory_order_acquire);
    if (__builtin_expect(value & (WRITER | UPGRADED), 0)) {
        bits_.fetch_add(-READER, std::memory_order_release);
        return false;
    }
    return true;
}

bool rw_spin_lock::try_unlock_upgrade_and_lock() {
    int32_t expect = UPGRADED;
    return bits_.compare_exchange_strong(expect, WRITER,
                                         std::memory_order_acq_rel);
}

bool rw_spin_lock::try_lock_upgrade() {
    int32_t value = bits_.fetch_or(UPGRADED, std::memory_order_acquire);
    return ((value & (UPGRADED | WRITER)) == 0);
}

