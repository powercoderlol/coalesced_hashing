#include <cassert>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <iostream>
#include <vector>

#include "coalesced_hashtable.hpp"

void simple_collision_test() {
    coalesced_hashtable_v1<int, int> chtable(2);

    std::vector<int> data_part_one{0, 8, 16, 24, 32};
    std::vector<int> data_part_two{64, 72};

    for(auto val : data_part_one) {
        chtable.insert(std::pair<int, int>(val, val));
    }

    for(int k = 100; k < 103; ++k) {
        chtable.insert(std::pair<int, int>(k, k));
    }
    for(auto val : data_part_two) {
        chtable.insert(std::pair<int, int>(val, val));
    }

    auto test = chtable.insert(std::pair<int, int>(8, 8));
    std::cout << test;
    assert(test);
}

int main() {
    simple_collision_test();
    return 0;
}
