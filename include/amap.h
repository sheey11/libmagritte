#ifndef MAGRITTE_FS_H
#define MAGRITTE_FS_H

class AllocationMap {
public:
  void set_empty(int i);
  void set_full(int i);
  bool get_full(int i);
  bool get_empty(int i);
  AllocationMap();
  ~AllocationMap();

private:
  char *table;
};

#endif
