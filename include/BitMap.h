#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

class BitMap {
  public:
    BitMap(int size);
    BitMap(BitMap& b);
    BitMap(BitMap&& bmp);
    BitMap& operator=(BitMap& bmp);
    static BitMap LoadExisting(std::vector<unsigned char>&& b);

    void Set(int i, bool v);
    bool Get(int i) const;
    int FindVacantAndSet();
    uint32_t count_vacant();
    const std::vector<unsigned char> GetUnderlayBytes();

  private:
    std::vector<unsigned char> store;
    int lastVacant;
    std::mutex vacantMutex;

    static uint32_t bitMask32(int i);
    static unsigned char fastLog2(unsigned char b);
};

