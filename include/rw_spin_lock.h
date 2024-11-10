#pragma once

// Reference:
// https://github.com/facebook/folly/blob/main/folly/synchronization/RWSpinLock.h

#include <atomic>
#include <cstdint>

class rw_spin_lock {
    enum : int32_t { READER = 4, UPGRADED = 2, WRITER = 1 };

  public:
    constexpr rw_spin_lock() : bits_(0) {};
    void lock();
    void unlock();
    void lock_shared();
    void unlock_shared();
    void lock_upgrade();
    void unlock_and_lock_shared();
    void unlock_upgrade();
    void unlock_upgrade_and_lock();
    void unlock_upgrade_and_lock_shared();
    void unlock_and_lock_upgrade();
    bool try_lock();
    bool try_lock_shared();
    bool try_unlock_upgrade_and_lock();
    bool try_lock_upgrade();

    int32_t bits() const { return bits_.load(std::memory_order_acquire); }

  private:
    std::atomic<int32_t> bits_;
};

