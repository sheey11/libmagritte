#include "BitMap.h"
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>

TEST(BitMapTest, FindVacantAndSetUntilFull) {
    int size = 1 << 5;

    auto size_str = std::getenv("MGRT_BITMAP_SIZE");
    if (size_str && strlen(size_str) != 0) {
        size = atoi(size_str);
    }

    int count = 0;

    BitMap bitmap(size);

    while (true) {
        int index = bitmap.FindVacantAndSet();
        if (index == -1) {
            EXPECT_EQ(count, size);
            break;
        } else {
            EXPECT_GE(index, 0);
            EXPECT_LT(index, size);
            count++;
        }
    }

    EXPECT_EQ(bitmap.count_vacant(), 0);
}
