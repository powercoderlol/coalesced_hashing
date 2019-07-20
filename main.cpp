#include <cassert>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <iostream>

#include "coalesced_hashtable.hpp"

template<class T, class Alloc>
class storage {
    using allocator_t = typename Alloc::template rebind<T>::other;

private:
    allocator_t storage_allocator;

public:
    void add_node() {
        auto test = storage_allocator.allocate(1);
    }
};

template<class T, class K, class Alloc = std::allocator<std::pair<T, K>>>
class my_container {
    using storage_ = storage<T, Alloc>;

private:
    storage_ stor;

public:
    void allocate_node() {
        stor.add_node();
    }
};

void simple_collision_test() {
    coalesced_hashtable_v1<int, int> chtable(10);

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

// template<class T>
// struct ch_node_t : hash_node {
//    template<class... Args>
//    ch_node_t(Args&&... args) : value(std::forward<Args>(args)...) {
//    }
//    T value;
//};

template<class T>
struct my_node {
    template<class... Args>
    my_node(Args&&... args) : value(std::forward<Args>(args)...) {
    }
    T value;
};

template<class T>
std::string to_string_impl(const T& t) {
    std::stringstream ss;
    ss << t;
    return ss.str();
}

template<class... Param>
std::vector<std::string> to_string(const Param&... param) {
    return {to_string(param)...};
}

using test = std::pair<std::string, double>;

int main() {

    //std::unordered_map<int, int> umap;
    simple_collision_test();

    return 0;
}
