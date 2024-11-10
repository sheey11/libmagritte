#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <magritte_impl.h>
#include <magritte_typedefs.h>
#include <ratio>
#include <string>
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

TEST(MagritteTest, BasicOperations) {
    auto file = "/tmp/libmgrt-test-file.mgrt";
    auto _fn = std::getenv("MGRT_PRESSURE_TEST_FILE");
    if (_fn && strlen(_fn)) {
        file = _fn;
    }

    std::remove(file);

    Magritte mgrt(file);
    auto data = generateData();

    MagritteKey key = 0x12345678;
    ASSERT_TRUE(mgrt.put(key, data));

    ASSERT_TRUE(mgrt.get(key, data));
    EXPECT_EQ(data.size(), 1024);

    for (size_t i = 0; i < 1024; ++i) {
        EXPECT_EQ(data[i], data[i]) << "Data mismatch at byte " << i;
    }

    std::atomic<uint64_t> write_nanosecs = 0;
    std::atomic<uint64_t> read_nanosecs = 0;
    std::vector<std::thread> threads;

    int n_test = 100000;

    auto n_test_str = std::getenv("MGRT_PRESSURE_TEST_SIZE");
    if (n_test_str && strlen(n_test_str) != 0) {
        n_test = atoi(n_test_str);
    }
    auto enable_speed_couting = n_test < 500000;
    if (!enable_speed_couting) {
        std::cout << "! NOTE: test size is too large, r/w speed measure "
                     "disabled due to floating point inaccuracy."
                  << std::endl;
    }

    const int n_threads = std::thread::hardware_concurrency();

    std::atomic<int32_t> curr_key = 0;
    std::cout << "Start pressuring libmagritte using "
              << std::to_string(n_threads) << " threads." << std::endl;

    std::chrono::time_point<
        std::chrono::system_clock,
        std::chrono::duration<long, std::ratio<1, 1000000000>>>
        start, finish;

    for (int i = 0; i < n_threads; ++i) {
        threads.emplace_back([&, i] {
            auto key = curr_key++;
            while (key < n_test) {
                auto data = generateData();
                auto put_result = false;

                if (enable_speed_couting)
                    start = std::chrono::high_resolution_clock::now();

                put_result = mgrt.put(key, data);

                if (enable_speed_couting) {
                    finish = std::chrono::high_resolution_clock::now();
                    auto microseconds =
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            finish - start);

                    write_nanosecs += microseconds.count();
                }

                ASSERT_TRUE(put_result);

                // test read
                std::vector<char> readData;
                auto read_result = false;

                if (enable_speed_couting)
                    start = std::chrono::high_resolution_clock::now();

                read_result = mgrt.get(key, readData);

                if (enable_speed_couting) {
                    finish = std::chrono::high_resolution_clock::now();
                    auto microseconds =
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            finish - start);

                    read_nanosecs += microseconds.count();
                }
                ASSERT_TRUE(read_result);

                for (size_t j = 0; j < 1024; ++j) {
                    EXPECT_EQ(data[j], readData[j])
                        << "Data mismatch at byte " << j << ", key " << key;
                }

                if (key % 100 == 0 || key == n_test) {
                    std::cout << "[" << std::to_string(key * 100 / n_test)
                              << "%] current key: " << key << "\r";
                    std::cout.flush();
                }

                key = curr_key++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << std::endl;
    if (enable_speed_couting) {
        std::cout << "Average time per eration: write: "
                  << write_nanosecs / n_test
                  << " ns / read: " << read_nanosecs / n_test << " ns"
                  << std::endl;
        std::cout << "Write speed: "
                  << double(n_test) / (double(write_nanosecs) * 1e-9) / 1024
                  << " MB/s" << std::endl;
        std::cout << "Read speed: "
                  << double(n_test) / (double(read_nanosecs) * 1e-9) / 1024
                  << " MB/s" << std::endl;
    }

    mgrt.shutdown();
}
