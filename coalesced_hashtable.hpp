#pragma once
// clang-format off
// Implementation of hash table using coalesced hashing algorithms
// Based on Jeffrey Scott Vitter article
// https://www.researchgate.net/publication/220424188_Implementations_for_Coalesced_Hashing

#include <cstdint>
#include <memory>
#include <type_traits>
// clang-format on

namespace {

struct address_node_t {
    uint32_t prev = 0;
    uint32_t next = 0;
};

struct address_node_traits {
    using node_type = address_node_t;
    enum {
        tail_flag = 0x80000000,
        head_flag = 0x40000000,
        allocated_flag = 0x20000000,
        all = 0xE0000000
    };
    static inline uint32_t next(node_type* x) {
        return x->next;
    }
    static inline uint32_t prev(node_type* x) {
        return x->prev & ~all;
    }
    static inline void set_next(node_type* x, uint32_t pos) {
        x->next = pos;
    }
    static inline void set_prev(node_type* x, uint32_t pos) {
        x->prev = pos | (x->prev & all);
    }
    static inline void set_tail(node_type* x) {
        x->prev |= tail_flag;
    }
    static inline void set_head(node_type* x) {
        x->prev |= head_flag;
    }
    static inline void set_allocated(node_type* x) {
        x->prev |= allocated_flag;
    }
    static inline bool is_tail(node_type* x) {
        return 0 != (x->prev & tail_flag);
    }
    static inline bool is_head(node_type* x) {
        return 0 != (x->prev & head_flag);
    }
    static inline bool is_intermediate(node_type* x) {
        return (!is_head(x) && !is_tail(x));
    }
    static inline bool is_allocated(node_type* x) {
        return 0 != (x->prev & allocated_flag);
    }
    static inline void reset_flags(node_type* x) {
        x->prev &= ~all;
    }
    static inline void reset_tail(node_type* x) {
        x->prev &= ~tail_flag;
    }
    static inline void link_(
        node_type* n, node_type* p, uint32_t n_pos, uint32_t p_pos) {
        set_allocated(n);
        set_allocated(p);
        set_next(p, n_pos);
        set_prev(n, p_pos);
        if(is_tail(p))
            reset_tail(p);
        set_tail(n);
    }
    static inline void link_head(node_type* x, uint32_t pos) {
        link_(x, x, pos, pos);
        set_head(x);
        set_tail(x);
    }
};

template<class Key, class T>
struct ch_node_t : address_node_t {
    using value_type = std::pair<const Key, T>;
    template<class... Args>
    ch_node_t(Args&&... args) : value(std::forward<Args>(args)...) {
    }
    value_type value;
};

// TODO: move allocator rebind to traits?
template<class Key, class T>
struct ch_node_traits : address_node_traits {
    using node_type = ch_node_t<Key, T>;
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;

    static const key_type& key(const node_type& node) {
        return (node.value.first);
    }

    static const mapped_type& value(const node_type& node) {
        return (node.value.second);
    }

    static const key_type& key(const value_type& data) {
        return (data.first);
    }
};

} // namespace

namespace coalesced_hash {

/** Insert mode
 * Colliding elements are stored in the same table.
 * References create chains which are subject to so called coalescence.
 *
 * LISCH (late insert standard coalesced hashing)
 * EISCH (early insert standard coalesced hashing)
 * using additional cellar space
 * LICH (late insert coalesced hashing)
 * EICH (early insert coalesced hashing)
 * VICH (variable insert coalesced hashing) */
enum class coalesced_insertion_mode { LISCH, EISCH, LICH, EICH, VICH };

// contiguous memory storage for coalesced hashtable
template<class Node, class Alloc = std::allocator<Node>>
class coalesced_hashtable {
    template<class Key, class T, class Hasher, class KeyEq, class Alloc>
    friend class coalesced_map;

    using node_type = Node;
    using storage_ptr = node_type*;

    using allocator_t =
        typename std::allocator_traits<Alloc>::template rebind_alloc<node_type>;
    using allocator_traits = std::allocator_traits<allocator_t>;

public:
    coalesced_hashtable() = delete;
    coalesced_hashtable(coalesced_hashtable& other) = delete;
    explicit coalesced_hashtable(
        size_t size,
        coalesced_insertion_mode mode = coalesced_insertion_mode::LISCH,
        double address_factor = 0.86)
        : capacity_(size)
        , address_factor_(address_factor)
        , insertion_mode_(mode) {
        // TODO: rounding for parameters
        // TODO: asserts
        address_region_ = static_cast<size_t>(capacity_ * address_factor_);
        cellar_ = static_cast<size_t>(capacity_ - address_region_);
        table_ = allocator_traits::allocate(allocator_, capacity_ + 1);
        freetail_ = static_cast<uint32_t>(capacity_ - 1);
        head_ = static_cast<uint32_t>(capacity_);
        tail_ = head_;
    }
    ~coalesced_hashtable() {
        // ensure all objects was destructed
        allocator_traits::deallocate(allocator_, table_, capacity_);
    }

    template<class... Args>
    void construct_node(storage_ptr ptr, Args&&... args) {
        ++size_;
        allocator_traits::construct(
            allocator_, ptr, std::forward<Args>(args)...);
    }

    void release_node(storage_ptr ptr) {
        --size_;
        allocator_traits::destroy(allocator_, ptr);
    }

    storage_ptr get_node(size_t pos) {
        return &table_[pos];
    }

    bool head_initialized() {
        return (head_ != capacity_);
    }

    storage_ptr get_head() {
        if(head_initialized())
            return table_[head_];
        return nullptr;
    }

    storage_ptr get_tail() {
        return table_[tail_];
    }

private:
    allocator_t allocator_;
    storage_ptr table_{nullptr};
    storage_ptr freelist_{nullptr};

    coalesced_insertion_mode insertion_mode_;
    double address_factor_{0.86};
    size_t cellar_{0};
    size_t address_region_{0};
    size_t capacity_{0};

    uint32_t freetail_{0};
    uint32_t head_{0};
    uint32_t tail_{0};
    size_t size_{0};
};

// TODO: const iterator
template<class Node, class Traits>
class ch_iterator_t {
public:
    using node_type = Node;
    using node_traits = Traits;

    using iterator_category = std::forward_iterator_tag;
    using value_type = typename node_traits::value_type;
    using difference_type = ptrdiff_t;
    using pointer = node_type*;
    using reference = node_type&;

    using storage_type = coalesced_hashtable<node_type>;

    ch_iterator_t() = default;
    ch_iterator_t(const storage_type& stor, const pointer& p)
        : storage_(stor), node_(p) {
    }

    ch_iterator_t& operator++() {
        auto pos = node_traits::next(node_);
        node_ = storage_->get_node(pos);
        return (*this);
    }

    reference operator*() const {
        return (*node_);
    }

    pointer operator->() const {
        return node_;
    }

    bool operator!=(const ch_iterator_t& rhs) const {
        return (node_.value.second != rhs.value.second);
    }

    bool operator==(const ch_iterator_t& rhs) const {
        return !(*this != rhs);
    }

private:
    const storage_type* storage_{nullptr};
    pointer node_{nullptr};
};

template<
    class Key, class T, class Hasher = std::hash<Key>,
    class KeyEq = std::equal_to<Key>,
    class Alloc = std::allocator<std::pair<const Key, T>>>
class coalesced_map {
    using key_equal = KeyEq;
    using hasher = Hasher;
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using node_type = ch_node_t<Key, T>;
    using node_traits = ch_node_traits<Key, T>;
    using allocator_t =
        typename std::allocator_traits<Alloc>::template rebind_alloc<node_type>;

    using pointer = typename std::allocator_traits<allocator_t>::pointer;
    using const_pointer =
        typename std::allocator_traits<allocator_t>::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using size_type = size_t;
    using difference_type = size_t;

    using iterator_t = ch_iterator_t<node_type, node_traits>;
    using const_iterator_t = ch_iterator_t<node_type, node_traits>;
    using iterator = ch_iterator_t<node_type, node_traits>;
    // using iterator = std::conditional_t<
    //     std::is_same_v<key_type, value_type>, typename iterator_t,
    //     typename const_iterator_t>;
    // using const_iterator = implementation-defined ;
    // using local_iterator = implementation-defined ;
    // using const_local_iterator = implementation-defined ;

    using storage_type = coalesced_hashtable<node_type, Alloc>;

public:
    coalesced_map() = delete;
    coalesced_map(coalesced_map& other) = delete;
    coalesced_map(size_t size) : storage_(size) {
        link_freelist(storage_);
    }

    size_t size() const {
        return size_;
    }

    // [[nodiscard]] iterator begin() {
    //     iterator(storage_, storage_.head_);
    // }
    //
    // [[nodiscard]] iterator end() {
    //     iterator(storage_, storage_.end_);
    // }

    // TODO: return iterator
    // TODO: check insertion mode (storage_)
    bool insert(value_type&& data) {
        auto key_ = node_traits::key(data);
        auto slot_ = get_slot_(key_);
        auto node = &storage_.table_[slot_];
        if(!node_traits::is_allocated(node)) {
            construct(node, data);
            // bucket
            node_traits::link_head(node, slot_);
            if(!storage_.head_initialized())
                storage_.head_ = slot_;
            link_to_table_tail(slot_);
            ++buckets_size_;
            return true;
        }
        // search for bucket tail
        while(!node_traits::is_tail(node)) {
            slot_ = node_traits::next(node);
            node = storage_.get_node(slot_);
        }
        switch(storage_.insertion_mode_) {
        case coalesced_insertion_mode::VICH:
            [[fallthrough]];
        case coalesced_insertion_mode::LICH:
            [[fallthrough]];
        case coalesced_insertion_mode::EICH:
            [[fallthrough]];
        case coalesced_insertion_mode::EISCH:
            [[fallthrough]];
        case coalesced_insertion_mode::LISCH:
            // cellar_ + address_region_ late insert
            auto free_index = static_cast<uint32_t>(storage_.freetail_);
            auto candidate_node = &storage_.table_[free_index];
            if(free_index > 0) {
                while(free_index > 0) {
                    candidate_node = &storage_.table_[free_index];
                    if(node_traits::is_allocated(candidate_node)) {
                        --free_index;
                        continue;
                    }
                    construct(candidate_node, data);
                    auto next_node_pos = node_traits::next(node);
                    auto next_node = storage_.get_node(next_node_pos);
                    node_traits::link_(candidate_node, node, free_index, slot_);
                    if(!node_traits::is_allocated(next_node))
                        link_to_table_tail(free_index);
                    else {
                        node_traits::set_prev(next_node, free_index);
                        node_traits::set_next(candidate_node, next_node_pos);
                    }
                    storage_.freetail_ = --free_index;
                    return true;
                }
            }
            break;
        }
        return false;
    }

private:
    static void link_freelist(storage_type& stor) {
        auto* table = stor.table_;
        auto border = (stor.capacity_ - 1);
        for(int i = 1; i < border; ++i) {
            table[i].next = i + 1;
            table[i].prev = i - 1;
        }
        table[0].prev = 0;
        table[0].next = 1;
        table[border].prev = static_cast<uint32_t>(border - 1);
        table[border].next = static_cast<uint32_t>(border);
    }

    void link_to_table_tail(uint32_t pos) {
        auto node = &storage_.table_[pos];
        auto tail_node = &storage_.table_[storage_.tail_];
        if(!node_traits::is_allocated(tail_node)) {
            // raw construct call to keep size valid
            storage_.construct_node(tail_node);
            node_traits::set_allocated(tail_node);
            node_traits::set_next(node, storage_.tail_);
            node_traits::set_prev(tail_node, pos);
            return;
        }
        auto actual_tail = &storage_.table_[node_traits::prev(tail_node)];
        node_traits::set_next(node, storage_.tail_);
        node_traits::set_prev(node, node_traits::prev(tail_node));
        node_traits::set_prev(tail_node, pos);
        node_traits::set_next(actual_tail, pos);
    }

    uint32_t get_slot_(const key_type& key) {
        return static_cast<uint32_t>(hash_(key));
    }

    size_type hash_(const key_type& key) {
        return hasher{}(key) % storage_.address_region_;
    }

    template<class... Args>
    void construct(node_type* ptr, Args&&... args) {
        ++size_;
        storage_.construct_node(ptr, std::forward<Args>(args)...);
    }

private:
    storage_type storage_;
    size_t buckets_size_{0};
    size_t size_{0};
};

} // namespace coalesced_hash
