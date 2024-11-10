#include "index_cluster.h"
#include <cstdlib>
#include <fcntl.h>
#include <gtest/gtest.h>

const std::string tempFilePath = "/tmp/libmgrt-index-cluster-test-file";

TEST(IndexCluster, BasicOperations) {
    if(access(tempFilePath.c_str(), F_OK) != -1) {
        remove(tempFilePath.c_str());
    }

    int file = open(tempFilePath.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    {
        IndexCluster cluster(file, 0, 0);

        for (MagritteKey key = -1024; key < 1024; key++) {
            MagritteIndex index = abs(key);
            cluster.put(key, index);
        }

        ASSERT_EQ(cluster.size(), 2);

        // try overwrite some
        for (MagritteKey key = -512; key < 512; key += 2) {
            MagritteIndex index = abs(key) * 3;
            cluster.put(key, index);
        }

        cluster.flush(FlushReason::Manually);

        for (MagritteKey key = -1024; key < 1024; key++) {
            MagritteIndex index;
            ASSERT_TRUE(cluster.get(key, index)) << "key: " << key;
            if (key < 512 && key >= -512 && (key + 512) % 2 == 0)
                ASSERT_EQ(index, abs(key) * 3) << "key: " << key;
            else
                ASSERT_EQ(index, abs(key)) << "key: " << key;
        }
    }

    {
        IndexCluster cluster(file, 0, 2);
        for (MagritteKey key = -1024; key < 1024; key++) {
            MagritteIndex index;
            ASSERT_TRUE(cluster.get(key, index)) << "key: " << key;
            if (key < 512 && key >= -512 && (key + 512) % 2 == 0)
                ASSERT_EQ(index, abs(key) * 3) << "key: " << key;
            else
                ASSERT_EQ(index, abs(key)) << "key: " << key;
        }
    }
}
