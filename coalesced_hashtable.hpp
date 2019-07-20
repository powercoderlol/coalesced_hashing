#pragma once

#include <cstdint>
#include <vector>

template<class T>
inline void hash_combine(std::size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

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
        allocated_flag = 0x10000000,
        all = 0xF0000000
    };
    static inline uint32_t next(node_type& x) {
        return x.next;
    }
    static inline uint32_t prev(node_type& x) {
        return x.prev & ~all;
    }
    static inline void set_next(node_type& x, uint32_t pos) {
        x.next = pos;
    }
    static inline void set_prev(node_type& x, uint32_t pos) {
        x.prev = pos | (x.prev & all);
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
        return 0 != (x.prev & tail_flag);
    }
    static inline bool is_head(node_type& x) {
        return 0 != (x.prev & head_flag);
    }
    static inline bool is_intermediate(node_type& x) {
        return 0 != (x.prev & intermediate_flag);
    }
    static inline bool is_allocated(node_type& x) {
        return 0 != (x.prev & allocated_flag);
    }
    static inline void reset_flags(node_type& x) {
        x.prev &= ~all;
    }
    static inline void reset_tail(node_type& x) {
        x.prev &= ~tail_flag;
    }
    static inline void link_(
        node_type& n, node_type& p, uint32_t n_pos, uint32_t p_pos) {
        set_next(p, n_pos);
        set_prev(n, p_pos);
        if(is_tail(p)) {
            reset_tail(p);
            if(!is_head(p))
                set_intermediate(p);
            set_tail(n);
        }
    }
    static inline void link_head(node_type& x, uint32_t pos) {
        link_(x, x, pos, pos);
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
    class KeyEqual = std::equal_to<Key>,
    class Alloc = std::allocator<std::pair<const Key, T>>>
class coalesced_hashtable_v1 {
    //  Colliding elements are stored in the same table. References
    //  create chains which are subject to so called coalescence.
    //
    //  LISCH (late insert standard coalesced hashing)
    //  EISCH (early insert standard coalesced hashing)
    //  using additional cellar space
    //  LICH (late insert coalesced hashing)
    //  EICH (early insert coalesced hashing)
    //  VICH (variable insert coalesced hashing)
    //
    enum class insertion_mode { LISCH, EISCH, LICH, EICH, VICH };

    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;

    using hasher = Hasher;
    using key_equal = KeyEqual;

    using node_type = ch_node_t<value_type>;
    using node_traits = hash_node_traits;
    // TODO: make it C++20 way
    // use allocator_traits instead
    using allocator_t = typename Alloc::template rebind<node_type>::other;

public:
    coalesced_hashtable_v1() = delete;
    coalesced_hashtable_v1(uint32_t size)
        : size_(size + 1), capacity_(size), freetail_(size), freelist_(size) {
        address_region_ = static_cast<uint32_t>(address_factor_ * size);
        cellar_region_ = size - address_region_;
        storage_ = storage_allocator_.allocate(size_);
    }
    ~coalesced_hashtable_v1() {
        // TODO: ensure all nodes was deallocated
        storage_allocator_.deallocate(storage_, size_);
    }

private:
    uint32_t get_slot_(const key_type& val) const {
        auto slot = hasher{}(val) % address_region_;
        return static_cast<uint32_t>(slot + 1);
    }
    void static allocate_and_link_head_(node_type& node, uint32_t pos) {
        node_traits::set_allocated(node);
        node_traits::link_head(node, pos);
    }
    void static allocate_and_link_(
        node_type& n, node_type& p, uint32_t n_pos, uint32_t p_pos) {
        node_traits::set_allocated(n);
        node_traits::link_(n, p, n_pos, p_pos);
    }

public:
    // return std::pair<iterator, bool>
    bool insert(const value_type& v) {
        auto slot = get_slot_(v.first);
        auto node = &storage_[slot];
        if(!node_traits::is_allocated(*node)) {
            new(node) node_type(v);
            allocate_and_link_head_(*node, slot);
            --freelist_;
            return true;
        }
        if(key_eq_(v.first, node->value.first))
            return true;
        while(!node_traits::is_tail(*node)
              && node_traits::is_allocated(*node)) {
            slot = node_traits::next(*node);
            node = &storage_[slot];
            if(key_eq_(v.first, node->value.first))
                return true;
        }
        while(node_traits::is_allocated(storage_[freetail_])) {
            --freetail_;
            if(0 == freetail_) {
                return false;
            }
        }
        auto n = &storage_[freetail_];
        new(n) node_type(v);
        allocate_and_link_(*n, *node, freetail_, slot);
        --freelist_;
        return true;
    }

private:
    allocator_t storage_allocator_;
    node_type* storage_ = nullptr;
    size_t size_;
    key_equal key_eq_;

    uint32_t freetail_ = 0;
    uint32_t freelist_ = 0;
    uint32_t capacity_ = 0;
    uint32_t address_region_;
    uint32_t cellar_region_;

    double address_factor_ = 0.86;
    double load_factor_ = 1;
};
