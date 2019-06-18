// clang-format off
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <string>
#include <iostream>
#include <typeinfo>

#include "coalesced_hashtable.hpp"

#include "gtest/gtest.h"
// clang-format on

TEST(coalesced_hashtable_test, simple_insert) {
    auto test_size_ = 10;
    coalesced_hash::coalesced_map<int, int> cmap_(test_size_);
    EXPECT_EQ(cmap_.insert(std::make_pair<int, int>(2, 2)), true);
    EXPECT_EQ(cmap_.insert({2, 8}), true);
    EXPECT_EQ(cmap_.size(), 2);
    auto border_ = 100 + 8;
    for(int i = 100; i < border_; ++i) {
        EXPECT_EQ(cmap_.insert({i, i + 1}), true);
    }
    EXPECT_EQ(cmap_.insert({400, 20}), false);
    EXPECT_EQ(cmap_.insert({42, 42}), false);
}


int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}