#include <assert.h>

#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <xhash>

struct hash_node {
    uint32_t prev = 0;
    uint32_t next = 0;
};

struct hash_node_traits {
    using node_type = hash_node;
    enum {
        tail_flag = 0x80000000,
        head_flag = 0x40000000,
        intermediate_flag = 0x20000000,
        allocated_flag = 0x10000000
    };
    static inline uint32_t next(node_type& x) {
        return x.next;
    }
    static inline uint32_t prev(node_type& x) {
        return (x.prev & 0xFFFFFFF);
    }
    static inline void set_next(node_type& x, uint32_t pos) {
        x.next = pos;
    }
    static inline void set_prev(node_type& x, uint32_t pos) {
        x.prev = pos;
    }
    static inline void set_tail(node_type& x) {
        x.prev |= tail_flag;
    }
    static inline void set_head(node_type& x) {
        x.prev |= head_flag;
    }
    static inline void set_intermediate(node_type& x) {
        x.prev |= intermediate_flag;
    }
    static inline void set_allocated(node_type& x) {
        x.prev |= allocated_flag;
    }
    static inline bool is_tail(node_type& x) {
        return (x.prev & tail_flag);
    }
    static inline bool is_head(node_type& x) {
        return (x.prev & head_flag);
    }
    static inline bool is_intermediate(node_type& x) {
        return (x.prev & intermediate_flag);
    }
    static inline bool is_allocated(node_type& x) {
        return (x.prev & allocated_flag);
    }
    static inline void reset_flags(node_type& x) {
        x.prev &= 0xFFFFFFF;
    }
    static inline void reset_tail(node_type& x) {
        x.prev &= ~tail_flag;
    }
    static inline void link_(
        node_type& n, node_type& p, uint32_t n_pos, uint32_t p_pos) {
        set_next(p, n_pos);
        // set_prev(n, p_pos);
        if(is_tail(p)) {
            reset_tail(p);
            if(!is_head(p))
                set_intermediate(p);
        }
        set_tail(n);
        set_allocated(n);
    }
    static inline void link_head(node_type& x) {
        set_allocated(x);
        set_head(x);
        set_tail(x);
    }
};

template<class T>
struct ch_node_t : hash_node {
    template<class... Args>
    ch_node_t(Args&&... args) : value(std::forward<Args>(args)...) {
    }
    T value;
};

template<
    class Key, class T, class Hasher = std::hash<Key>,
    class KeyEq = std::equal_to<Key>>
//    class Alloc = std::allocator<std::pair<Key, T>>>
class coalesced_hashtable_v1 {
    using node_type = ch_node_t<T>;
    using node_traits = hash_node_traits;
    using key_type = Key;
    using value_type = T;

public:
    coalesced_hashtable_v1() = delete;
    // coalesced_hashtable_v1(Alloc& alloc) {
    //}
    coalesced_hashtable_v1(uint32_t size)
        : size_(size + 1), capacity_(size), freetail_(size), freelist_(size) {
        address_region_ = static_cast<uint32_t>(address_factor_ * size);
        cellar_region_ = size - address_region_;
        // search_length_ = linear_probing_factor_ * size;
        // table_.reserve(size + 1);
        storage_ = storage_allocator_.allocate(size_);
    }
    ~coalesced_hashtable_v1() {
        storage_allocator_.deallocate(storage_, size_);
    }

private:
    uint32_t get_slot_(const value_type& val) const {
        auto slot = Hasher{}(val) % address_region_;
        return static_cast<uint32_t>(slot + 1);
    }

public:
    bool insert(const value_type& value) {
        auto slot = get_slot_(value);
        auto node = &storage_[slot];
        if(!node_traits::is_allocated(*node)) {
            new(node) node_type(value);
            node_traits::link_head(*node);
            --freelist_;
            return true;
        }
        if(node->value == value) {
            return true;
        }
        while(!node_traits::is_tail(*node)
              && node_traits::is_allocated(*node)) {
            slot = node_traits::next(*node);
            node = &storage_[slot];
            if(node->value == value)
                return true;
        }
        // collision: add into cellar
        while(node_traits::is_allocated(storage_[freetail_])) {
            --freetail_;
            // table overloaded
            // rehash
            if(0 == freetail_)
                return false;
        }
        auto n = &storage_[freetail_];
        new(n) node_type(value);
        node_traits::link_(*n, *node, freetail_, slot);
        --freelist_;
        return true;
    }

private:
    // std::vector<node_type, std::allocator<node_type>> table_;
    std::allocator<node_type> storage_allocator_;
    size_t size_;
    node_type* storage_ = nullptr;

    uint32_t freetail_ = 0;
    uint32_t freelist_ = 0;
    uint32_t capacity_ = 0;
    uint32_t address_region_;
    uint32_t cellar_region_;
    uint32_t search_length_;

    double address_factor_ = 0.86;
    double load_factor_ = 1;
    // double linear_probing_factor_ = 0.05;
};

int main() {
    coalesced_hashtable_v1<int, int> chtable(10);

    std::vector<uint32_t> data_part_one{0, 8, 16, 24, 32};
    std::vector<uint32_t> data_part_two{64, 72};

    for(auto val : data_part_one) {
        chtable.insert(val);
    }
    for(size_t k = 100; k < 103; ++k) {
        chtable.insert((uint32_t)k);
    }
    for(auto val : data_part_two) {
        chtable.insert(val);
    }

    auto test = chtable.insert(8);
    assert(test);

    return 0;
}
