#include "lru.h"
#include <gtest/gtest.h>

// Test written by ChatGPT.
TEST(LRUCacheTest, BasicOperations) {
    LRUCache<int, std::string> cache(2);

    // Test insertion and retrieval
    cache.put(1, "one");
    cache.put(2, "two");

    std::string value;
    EXPECT_TRUE(cache.get(1, value));
    EXPECT_EQ(value, "one");

    EXPECT_TRUE(cache.get(2, value));
    EXPECT_EQ(value, "two");

    // Test eviction policy
    cache.put(3, "three"); // This should evict key 1
    EXPECT_FALSE(cache.get(1, value));
    EXPECT_TRUE(cache.get(3, value));
    EXPECT_EQ(value, "three");

    // Test updating existing key
    cache.put(2, "TWO");
    EXPECT_TRUE(cache.get(2, value));
    EXPECT_EQ(value, "TWO");

    // Adding another item should evict the least recently used (key 3)
    cache.put(4, "four");
    EXPECT_FALSE(cache.get(3, value));
    EXPECT_TRUE(cache.get(4, value));
    EXPECT_EQ(value, "four");
    EXPECT_TRUE(cache.get(2, value));
    EXPECT_EQ(value, "TWO");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
