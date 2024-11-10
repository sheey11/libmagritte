#include "index_block.h"
#include <cassert>

#include <climits>
#include <cstdlib>
#include <gtest/gtest.h>

TEST(IndexBlockTest, BasicOperations) {
    IndexBlock block(INT_MIN, INT_MAX);
    MagritteIndex index;
    for (MagritteKey i = -100; i <= 100; i++) {
        block.put(i, abs(i));
    }

    // try overwrite some
    for (MagritteKey i = -50; i < -20; i += 2) {
        block.put(i, static_cast<MagritteIndex>(-2 * i));
    }

    auto higher = block.split();
    ASSERT_EQ(higher.lower_bound(), 0);
    ASSERT_EQ(higher.upper_bound(), INT_MAX);
    ASSERT_EQ(block.lower_bound(), INT_MIN);
    ASSERT_EQ(block.upper_bound(), 0);
    ASSERT_EQ(higher.size(), 101); // -100, -99, ..., 0
    ASSERT_EQ(block.size(), 100);  // 1, 2, .., 100

    for (MagritteKey i = -100; i <= 150; i++) {
        if (i >= 0 && i <= 100) {
            ASSERT_TRUE(higher.get(i, index)) << "i: " << i;
            ASSERT_EQ(index, i);
            continue;
        }

        if (i <= 100) {
            ASSERT_TRUE(block.get(i, index));
            if (i < -20 && i >= -50 && (i + 50) % 2 == 0)
                ASSERT_EQ(index, -2 * i);
            else
                ASSERT_EQ(index, abs(i));
            continue;
        }

        ASSERT_FALSE(higher.get(i, index));
    }

    block.remove(-50, index);
    ASSERT_FALSE(block.get(-50, index));
}
