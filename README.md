# libmagritte

小对象 KV 存储系统。

- 采用 CXX20 标准，使用了 STL 里的许多容器替代手动实现。
- 0 内存动态分配（`std::vector` 分配的不算），大量使用移动语义，无内存泄漏。
- 使用 CMake 构建。
- 使用了 `clangd` 作为 LSP、`clang-format` 作为格式化工具。
- 在 Arch Linux 上开发，内核版本 `6.11.6`。

## 构建

```sh
mkdir build && cd build/
cmake ..
cmake --build .

file src/libmagritte.so
```

## 运行单元测试

```sh
mkdir build && cd build/
cmake -DENABLE_TESTING=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel 16
ctest .
```

## 跑分

```
cd build/
./tests/magritte_test
```

### 调试

```sh
cmake -DENABLE_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDEBUG=ON ..
```
