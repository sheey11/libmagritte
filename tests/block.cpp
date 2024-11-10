#include "Block.h"
#include <atomic>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

MagritteValue generateData() {
    MagritteValue buffer(1024);
    std::ifstream randomFile("/dev/urandom", std::ios::in | std::ios::binary);
    if (randomFile.is_open()) {
        randomFile.read(buffer.data(), buffer.size());
    }
    return buffer;
}

TEST(BlockTest, BasicOperations) {
    auto fn = "/tmp/libmgrt-block-test-file";
    auto _fn = std::getenv("MGRT_PRESSURE_TEST_FILE");
    if (_fn && strlen(_fn)) {
        fn= _fn;
    }

    int file = open(fn, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (file < 0) {
        FAIL() << "Failed to create file at " << fn;
    }

    // Pre-allocate file space
    if (ftruncate(file, (1 << 30) + (1 << 17)) != 0) {
        close(file);
        FAIL() << "Failed to truncate file at " << fn;
    }

    Block block(file, 0);
    auto data = generateData();

    MagritteInBlockIndex idx;
    ASSERT_TRUE(block.put(data, idx));

    block.flush_sync();

    auto readData = block.get(idx);
    EXPECT_EQ(readData.size(), 1024);

    for (size_t i = 0; i < 1024; ++i) {
        EXPECT_EQ(data[i], readData[i]) << "Data mismatch at byte " << i;
    }

    std::atomic<uint64_t> totalNanosecons = 0;
    std::vector<std::thread> threads;
    const int nTest = 10000;
    threads.reserve(nTest);

    for (int i = 0; i < nTest; ++i) {
        threads.emplace_back([&, i] {
            MagritteInBlockIndex idx;
            auto put_result = false;
            auto data = generateData();

            auto start = std::chrono::high_resolution_clock::now();
            put_result = block.put(data, idx);
            auto finish = std::chrono::high_resolution_clock::now();
            auto microseconds =
                std::chrono::duration_cast<std::chrono::nanoseconds>(finish -
                                                                     start);

            totalNanosecons += microseconds.count();

            ASSERT_TRUE(put_result);

            if (i % 2 == 0) {
                block.flush_sync();
            }

            auto readData = block.get(idx);
            for (size_t j = 0; j < 1024; ++j) {
                EXPECT_EQ(data[j], readData[j])
                    << "Data mismatch at byte " << j << ", index " << idx;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Average time per eration: " << totalNanosecons / nTest
              << " ns" << std::endl;
    std::cout << "Write speed: "
              << double(nTest) / (double(totalNanosecons) * 1e-9) / 1024 << " MB/s"
              << std::endl;

}
