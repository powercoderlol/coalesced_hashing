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
    auto iter = cmap_.insert(std::make_pair<int, int>(2, 2));
    EXPECT_EQ(iter.second, true);
    EXPECT_EQ(cmap_.insert({2, 8}).second, true);
    EXPECT_EQ(cmap_.size(), 2);
    auto border_ = 100 + 8;
    for(int i = 100; i < border_; ++i) {
        EXPECT_EQ(cmap_.insert({i, i + 1}).second, true);
    }
    EXPECT_EQ(cmap_.insert({400, 20}).second, false);
    EXPECT_EQ(cmap_.insert({42, 42}).second, false);
}

TEST(coalesced_hashtable_test, find_member_method) {
    coalesced_hash::coalesced_map<int, int> cmap_(10);
    cmap_.insert({2, 8});
    cmap_.insert({3, 10});
    cmap_.insert({9, 12});
    auto test_it = cmap_.find(3);
    EXPECT_EQ(test_it->value.second, 10);
}

TEST(coalesced_hashtable_test, stl_find_if) {
    coalesced_hash::coalesced_map<int, int> cmap_(10);
    cmap_.insert({2, 8});
    cmap_.insert({3, 10});
    cmap_.insert({9, 12});
    auto test_it = std::find_if(
        cmap_.begin(), cmap_.end(),
        [](const auto& node) { return node.value.first == 3; });
    EXPECT_EQ(test_it->value.second, 10);
}

TEST(coalesced_hashtable_test, early_insertion) {
    coalesced_hash::coalesced_map<int, int> cmap_(
        10, coalesced_hash::coalesced_insertion_mode::EICH);
    cmap_.insert({3, 10});
    cmap_.insert({9, 12});
    cmap_.insert({2, 42});
    cmap_.insert({2, 420});
    cmap_.insert({2, 227});
    cmap_.insert({2, 5});
    for(const auto& val : cmap_) {
        std::cout << val.value.first << " " << val.value.second << '\n';
    }
    auto iter = cmap_.find(2);
    EXPECT_EQ(iter->value.second, 42);
    ++iter;
    EXPECT_EQ(iter->value.second, 5);
    ++iter;
    EXPECT_EQ(iter->value.second, 227);
    ++iter;
    EXPECT_EQ(iter->value.second, 420);
    EXPECT_EQ(coalesced_hash::address_node_traits::is_tail(&*iter), true);
}

// TODO: performance tests

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
