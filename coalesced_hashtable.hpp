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
#pragma once

#include "hash_traits.hpp"
#include <vector>

template <class T>
struct ch_node_t:hash_node {
	template<class... Args>
	ch_node_t(Args&&... args) : value(std::forward<Args>(args)...) {
	}
	T value;
};

template<
    class Key, 
	class T, 
	class Hasher = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Alloc = std::allocator<std::pair<const Key, T>>,
	class node_traits = hash_node_traits>
class coalesced_hashtable_v1 {
    
    enum class insertion_mode { LISCH, EISCH, LICH, EICH, VICH };

    using key_type = Key;
    using value_type = std::pair<const Key, T>;

    using hasher = Hasher;
    using key_equal = KeyEqual;

    using node_type = ch_node_t<value_type>;
    // TODO: make it C++20 way use allocator_traits instead
    using allocator_t = typename Alloc::template rebind<node_type>::other;

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

	coalesced_hashtable_v1() = delete;

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
    
    explicit coalesced_hashtable_v1(uint32_t size = 0) : 
	    size_(size + 1), 
		capacity_(size), 
		freetail_(size), 
		freelist_(size)
	{
        address_region_ = static_cast<uint32_t>(address_factor_ * size);
        cellar_region_ = size - address_region_;
        storage_ = storage_allocator_.allocate(size_);
    }
    ~coalesced_hashtable_v1() {
        // TODO: ensure all nodes was deallocated
        storage_allocator_.deallocate(storage_, size_);
    }

public:
   
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
};
