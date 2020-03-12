#pragma once
#include <cstdint>

struct hash_node {
	uint32_t prev = 0;
	uint32_t next = 0;
};

struct hash_node_traits {
	enum {
		tail_flag = 0x80000000,
		head_flag = 0x40000000,
		intermediate_flag = 0x20000000,
		allocated_flag = 0x10000000,
		all = 0xF0000000
	};

	static inline uint32_t next(hash_node& x) {
		return x.next;
	}
	static inline uint32_t prev(hash_node& x) {
		return x.prev & ~all;
	}
	static inline void set_next(hash_node& x, uint32_t pos) {
		x.next = pos;
	}
	static inline void set_prev(hash_node& x, uint32_t pos) {
		x.prev = pos | (x.prev & all);
	}
	static inline void set_tail(hash_node& x) {
		x.prev |= tail_flag;
	}
	static inline void set_head(hash_node& x) {
		x.prev |= head_flag;
	}
	static inline void set_intermediate(hash_node& x) {
		x.prev |= intermediate_flag;
	}
	static inline void set_allocated(hash_node& x) {
		x.prev |= allocated_flag;
	}
	static inline bool is_tail(hash_node& x) {
		return 0 != (x.prev & tail_flag);
	}
	static inline bool is_head(hash_node& x) {
		return 0 != (x.prev & head_flag);
	}
	static inline bool is_intermediate(hash_node& x) {
		return 0 != (x.prev & intermediate_flag);
	}
	static inline bool is_allocated(hash_node& x) {
		return 0 != (x.prev & allocated_flag);
	}
	static inline void reset_flags(hash_node& x) {
		x.prev &= ~all;
	}
	static inline void reset_tail(hash_node& x) {
		x.prev &= ~tail_flag;
	}

	static inline void link_(
		hash_node& n, hash_node& p, uint32_t n_pos, uint32_t p_pos) {
		set_next(p, n_pos);
		set_prev(n, p_pos);
		if (is_tail(p)) {
			reset_tail(p);
			if (!is_head(p))
				set_intermediate(p);
			set_tail(n);
		}
	}
	static inline void link_head(hash_node& x, uint32_t pos) {
		link_(x, x, pos, pos);
		set_head(x);
		set_tail(x);
	}
};