#pragma once
// clang-format off
// Implementation of hash table using coalesced hashing algorithms
// Based on Jeffrey Scott Vitter article
// https://www.researchgate.net/publication/220424188_Implementations_for_Coalesced_Hashing

#include <cstdint>
#include <memory>
#include <type_traits>
// clang-format on

namespace coalesced_hash {

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

    static const key_type& key(const node_type* node) {
        return (node->value.first);
    }

    static const mapped_type& value(const node_type* node) {
        return (node->value.second);
    }

    static const key_type& key(const value_type& data) {
        return (data.first);
    }
};

/** Insert mode
 * Colliding elements are stored in the same table.
 * References create chains which are subject to so called coalescence.
 *
 * LICH (late insert coalesced hashing)
 * EICH (early insert coalesced hashing)
 * VICH (variable insert coalesced hashing) */
enum class coalesced_insertion_mode { LICH, EICH, VICH };

// contiguous memory storage for coalesced hashtable
template<class Node, class Alloc>
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
        coalesced_insertion_mode mode = coalesced_insertion_mode::LICH,
        double address_factor = 0.86)
        : capacity_(size)
        , address_factor_(address_factor)
        , insertion_mode_(mode) {
        // TODO: rounding for parameters
        // TODO: asserts
        address_region_ = static_cast<size_t>(capacity_ * address_factor_);
        cellar_ = static_cast<size_t>(capacity_ - address_region_);
        table_ = allocator_traits::allocate(allocator_, capacity_ + 1);
        freetail_ = (mode == coalesced_insertion_mode::LICH)
            ? static_cast<uint32_t>(capacity_ - 1)
            : 0;
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
            return &table_[head_];
        return nullptr;
    }

    storage_ptr get_tail() {
        return &table_[tail_];
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
template<class Node, class Traits, class Storage>
class ch_iterator_t {
public:
    using node_type = Node;
    using node_traits = Traits;
    using storage_type = Storage;

    using iterator_category = std::forward_iterator_tag;
    using value_type = typename node_traits::value_type;
    using difference_type = ptrdiff_t;
    using node_pointer = node_type*;
    using node_reference = node_type&;

    ch_iterator_t() = default;
    ch_iterator_t(storage_type& stor, node_pointer p)
        : storage_(stor), node_(p) {
    }

    ch_iterator_t& operator++() {
        auto pos = node_traits::next(node_);
        node_ = storage_.get_node(pos);
        return (*this);
    }

    node_reference operator*() const {
        return (*node_);
    }

    node_pointer operator->() const {
        return node_;
    }

    bool operator!=(const ch_iterator_t& rhs) const {
        return (node_ != rhs.node_);
    }

    bool operator==(const ch_iterator_t& rhs) const {
        return !(*this != rhs);
    }

    ch_iterator_t& operator=(const ch_iterator_t& rhs) {
        if(rhs.node_ == node_)
            return (*this);
        node_ = rhs.node_;
        return (*this);
    }

private:
    storage_type& storage_{nullptr};
    node_pointer node_{nullptr};
};

// TODO: move mode to template parameter
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

    using storage_type = coalesced_hashtable<node_type, Alloc>;
    using iterator = ch_iterator_t<node_type, node_traits, storage_type>;

    // not multimap
    using pair_ib = std::pair<iterator, bool>;

public:
    coalesced_map() = delete;
    coalesced_map(coalesced_map& other) = delete;
    coalesced_map(
        size_t size,
        coalesced_insertion_mode mode = coalesced_insertion_mode::LICH,
        double address_factor = 0.86)
        : storage_(size, mode, address_factor) {
        // if(mode == coalesced_insertion_mode::EICH)
        //     link_freelist(storage_);
    }

    bool set_insertion_mode(coalesced_insertion_mode mode) {
        if(size_ > 0)
            return false;
        storage_.insertion_mode_ = mode;
        return true;
    }

    double load_factor() const {
        return float(size_) / float(buckets_count_);
    }

    double max_load_factor() const {
        return max_load_factor_;
    }

    void max_load_factor(double max_lf) {
        max_load_factor_ = max_lf;
    }

    size_t size() const {
        return size_;
    }

    [[nodiscard]] iterator begin() {
        return iterator(storage_, storage_.get_head());
    }

    [[nodiscard]] iterator end() {
        return iterator(storage_, storage_.get_tail());
    }

    [[nodiscard]] iterator find(const key_type& key) {
        auto slot = get_slot_(key);
        auto node = storage_.get_node(slot);
        if(!node_traits::is_head(node))
            return iterator(storage_, storage_.get_head());
        if(key_equal()(node_traits::key(node), key))
            return iterator(storage_, node);
        while(!node_traits::is_tail(node)) {
            slot = node_traits::next(node);
            node = storage_.get_node(slot);
            if(key_equal()(node_traits::key(node), key))
                return iterator(storage_, node);
        }
        return iterator(storage_, storage_.get_head());
    }

    // TODO: check insertion mode (storage_)
    pair_ib insert(value_type&& data) {
        auto key_ = node_traits::key(data);
        auto slot_ = get_slot_(key_);
        auto node = &storage_.table_[slot_];
        auto candidate_node = node;
        auto early_position = slot_;
        uint32_t free_index = 0;
        auto probe_counter = lookup_depth;
        if(!node_traits::is_allocated(node)) {
            construct_(node, data);
            node_traits::link_head(node, slot_);
            if(!storage_.head_initialized())
                storage_.head_ = slot_;
            link_to_table_tail(slot_);
            ++buckets_count_;
            return pair_ib(iterator(storage_, node), true);
        }
        // TODO: multimap
        // if(key_equal()(node_traits::key(node), key_))
        //    return pair_ib(iterator(storage_, node), true);
        while(!node_traits::is_tail(node)) {
            slot_ = node_traits::next(node);
            node = storage_.get_node(slot_);
            // TODO: multimap
            // if(key_equal()(node_traits::key(node), key_))
            //    return pair_ib(iterator(storage_, node), true);
        }
        switch(storage_.insertion_mode_) {
        case coalesced_insertion_mode::VICH:
            [[fallthrough]];
        case coalesced_insertion_mode::EICH:
            free_index = static_cast<uint32_t>(early_position);
            candidate_node = storage_.get_node(free_index);
            while(node_traits::is_allocated(candidate_node)
                  && (probe_counter != 0)) {
                ++free_index;
                --probe_counter;
                candidate_node = storage_.get_node(free_index);
            }
            if(!node_traits::is_allocated(candidate_node)) {
                construct_(candidate_node, data);
                auto root_node = storage_.get_node(early_position);
                auto next_node_pos = node_traits::next(root_node);
                auto next_node = storage_.get_node(next_node_pos);
                node_traits::link_(
                    candidate_node, root_node, free_index, early_position);
                if(!node_traits::is_allocated(next_node)) {
                    link_to_table_tail(free_index);
                }
                else {
                    node_traits::set_next(candidate_node, next_node_pos);
                    node_traits::set_prev(next_node, free_index);
                }
                return pair_ib(iterator(storage_, candidate_node), true);
            }
            free_index = static_cast<uint32_t>(storage_.freetail_);
            if(free_index >= storage_.capacity_)
                break;
            while(free_index < storage_.capacity_) {
                candidate_node = storage_.get_node(free_index);
                if(node_traits::is_allocated(candidate_node)) {
                    ++free_index;
                    continue;
                }
                construct_(candidate_node, data);
                auto root_node = storage_.get_node(early_position);
                auto next_node_pos = node_traits::next(root_node);
                auto next_node = storage_.get_node(next_node_pos);
                node_traits::link_(
                    candidate_node, root_node, free_index, early_position);
                if(!node_traits::is_allocated(next_node)) {
                    link_to_table_tail(free_index);
                }
                else {
                    node_traits::set_next(candidate_node, next_node_pos);
                    node_traits::set_prev(next_node, free_index);
                }
                return pair_ib(iterator(storage_, candidate_node), true);
            }
            break;
        case coalesced_insertion_mode::LICH:
            // cellar_ + address_region_ late insert
            free_index = static_cast<uint32_t>(storage_.freetail_);
            if(free_index > 0) {
                while(free_index > 0) {
                    candidate_node = storage_.get_node(free_index);
                    if(node_traits::is_allocated(candidate_node)) {
                        --free_index;
                        continue;
                    }
                    construct_(candidate_node, data);
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
                    return pair_ib(iterator(storage_, candidate_node), true);
                }
            }
            break;
        }
        return pair_ib(iterator(storage_, storage_.get_tail()), false);
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
    void construct_(node_type* ptr, Args&&... args) {
        ++size_;
        storage_.construct_node(ptr, std::forward<Args>(args)...);
        node_traits::set_allocated(ptr);
    }

    void rehash_() {
        // TODO
    }

private:
    storage_type storage_;
    size_t buckets_count_ = 0;
    size_t max_load_factor_ = 1;
    size_t size_ = 0;
    size_t lookup_depth = 2;
};

} // namespace coalesced_hash
