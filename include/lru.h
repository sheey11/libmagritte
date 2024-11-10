#pragma once

#include <cstddef>
#include <list>
#include <rw_spin_lock.h>
#include <unordered_map>

template <typename K, typename V> class LRUCache {
  public:
    LRUCache(size_t capacity);
    LRUCache<K, V>& operator=(LRUCache<K, V>&& other);
    bool get(const K& key, V& value);
    void put(const K& key, const V& value);
    void remove(const K& key);

  private:
    size_t capacity;
    std::list<std::pair<K, V>> cacheList;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator>
        cacheMap;
    rw_spin_lock lock;
};

template <typename K, typename V>
LRUCache<K, V>::LRUCache(size_t capacity) : capacity(capacity) {}

template <typename K, typename V>
LRUCache<K, V>& LRUCache<K, V>::operator=(LRUCache<K, V>&& other) {
    other.lock.lock();
    this->cacheList = std::move(other.cacheList);
    this->cacheMap = std::move(other.cacheMap);
    this->capacity = other.capacity;
    other.lock.unlock();

    return *this;
}

template <typename K, typename V>
bool LRUCache<K, V>::get(const K& key, V& value) {
    lock.lock_shared();

    auto it = cacheMap.find(key);
    if (it == cacheMap.end()) {
        lock.unlock_shared();
        return false;
    }
    cacheList.splice(cacheList.begin(), cacheList, it->second);
    value = it->second->second;

    lock.unlock_shared();
    return true;
}

template <typename K, typename V>
void LRUCache<K, V>::put(const K& key, const V& value) {
    lock.lock();
    auto it = cacheMap.find(key);
    if (it != cacheMap.end()) {
        it->second->second = value;
        cacheList.splice(cacheList.begin(), cacheList, it->second);
    } else {
        if (cacheList.size() == capacity) {
            auto last = cacheList.end();
            last--;
            cacheMap.erase(last->first);
            cacheList.pop_back();
        }
        cacheList.emplace_front(key, value);
        cacheMap[key] = cacheList.begin();
    }
    lock.unlock();
}

template <typename K, typename V> void LRUCache<K, V>::remove(const K& key) {
    lock.lock();
    auto it = cacheMap.find(key);
    if (it != cacheMap.end()) {
        cacheList.erase(it->second);
        cacheMap.erase(it);
    }
    lock.unlock();
}
