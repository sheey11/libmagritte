#include "amap.h"

void AllocationMap::set_empty(int i) {
  auto offset = i % 8;
  this->table[i / 8] &= (~(0b1 << offset));
}

void AllocationMap::set_full(int i) {
  auto offset = i % 8;
  this->table[i / 8] |= 0b1 << offset;
}

bool AllocationMap::get_full(int i) {
  auto offset = i % 8;
  return this->table[i / 8] & (0xb1 << offset);
}

bool AllocationMap::get_empty(int i) {
  return !this->get_full(i);
}

AllocationMap::AllocationMap() { this->table = new char[128 * 1024 * 1024]; }
AllocationMap::~AllocationMap() { delete[] this->table; }

