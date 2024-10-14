#include <gtest/gtest.h>
#include "amap.h"

TEST(AllocationTableTest, SetGet) {
	auto table = AllocationMap();
	table.set_full(114514);
	EXPECT_TRUE(table.get_full(114514));
}

