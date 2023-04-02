/*
 * \brief  Module for operating on the meta tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FREE_TREE_H_
#define _FREE_TREE_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>
#include <sha256_4k_hash.h>

namespace Cbe
{
	class Free_tree;
	class Free_tree_request;
	class Free_tree_channel;
}

class Cbe::Free_tree_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, ALLOC_FOR_NON_RKG = 1, ALLOC_FOR_RKG_CURR_GEN_BLKS = 2,
			ALLOC_FOR_RKG_OLD_GEN_BLKS = 3 };

	private:

		friend class Free_tree;
		friend class Free_tree_channel;

		Type             _type                    { INVALID };
		Genode::addr_t   _ft_root_pba_ptr         { 0 };
		Genode::addr_t   _ft_root_gen_ptr         { 0 };
		Genode::addr_t   _ft_root_hash_ptr        { 0 };
		Genode::uint64_t _ft_max_level            { 0 };
		Genode::uint64_t _ft_degree               { 0 };
		Genode::uint64_t _ft_leaves               { 0 };
		Genode::addr_t   _mt_root_pba_ptr         { 0 };
		Genode::addr_t   _mt_root_gen_ptr         { 0 };
		Genode::addr_t   _mt_root_hash_ptr        { 0 };
		Genode::uint64_t _mt_max_level            { 0 };
		Genode::uint64_t _mt_degree               { 0 };
		Genode::uint64_t _mt_leaves               { 0 };
		Genode::uint64_t _current_gen             { 0 };
		Genode::uint64_t _free_gen                { 0 };
		Genode::uint64_t _requested_blocks        { 0 };
		Genode::addr_t   _new_blocks_ptr          { 0 };
		Genode::addr_t   _old_blocks_ptr          { 0 };
		Genode::uint64_t _max_level               { 0 };
		Genode::uint64_t _vba                     { INVALID_VBA };
		Genode::uint64_t _vbd_degree              { 0 };
		Genode::uint64_t _vbd_highest_vba         { 0 };
		bool             _rekeying                { 0 };
		Genode::uint32_t _previous_key_id         { 0 };
		Genode::uint32_t _current_key_id          { 0 };
		Genode::uint64_t _rekeying_vba            { 0 };
		bool             _success                 { false };
		Genode::addr_t   _snapshots_ptr           { 0 };
		Generation       _last_secured_generation { INVALID_GENERATION };

	public:

		Free_tree_request() { }

		Free_tree_request(Genode::uint64_t  src_module_id,
		                  Genode::uint64_t  src_request_id,
		                  Genode::size_t    req_type,
		                  Genode::addr_t    ft_root_pba_ptr,
		                  Genode::addr_t    ft_root_gen_ptr,
		                  Genode::addr_t    ft_root_hash_ptr,
		                  Genode::uint64_t  ft_max_level,
		                  Genode::uint64_t  ft_degree,
		                  Genode::uint64_t  ft_leaves,
		                  Genode::addr_t    mt_root_pba_ptr,
		                  Genode::addr_t    mt_root_gen_ptr,
		                  Genode::addr_t    mt_root_hash_ptr,
		                  Genode::uint64_t  mt_max_level,
		                  Genode::uint64_t  mt_degree,
		                  Genode::uint64_t  mt_leaves,
		                  Snapshots const  *snapshots,
		                  Generation        last_secured_generation,
		                  Genode::uint64_t  current_gen,
		                  Genode::uint64_t  free_gen,
		                  Genode::uint64_t  requested_blocks,
		                  Genode::addr_t    new_blocks_ptr,
		                  Genode::addr_t    old_blocks_ptr,
		                  Genode::uint64_t  max_level,
		                  Genode::uint64_t  vba,
		                  Genode::uint64_t  vbd_degree,
		                  Genode::uint64_t  vbd_highest_vba,
		                  bool              rekeying,
		                  Genode::uint32_t  previous_key_id,
		                  Genode::uint32_t  current_key_id,
		                  Genode::uint64_t  rekeying_vba);

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Genode::Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Cbe::Free_tree_channel
{
	private:

		friend class Free_tree;

		using Request = Free_tree_request;

		enum State {
			INVALID,
			SCAN,
			SCAN_COMPLETE,
			UPDATE,
			UPDATE_COMPLETE,
			COMPLETE,
			NOT_ENOUGH_FREE_BLOCKS,
			TREE_HASH_MISMATCH
		};

		struct Type_1_info
		{
			enum State {
				INVALID, AVAILABLE, READ, WRITE, COMPLETE };

			State       state   { INVALID };
			Type_1_node node    { };
			Node_index  index   { INVALID_NODE_INDEX };
			bool        volatil { false };
		};

		struct Type_2_info
		{
			enum State {
				INVALID, AVAILABLE, READ, WRITE, COMPLETE };

			State       state { INVALID };
			Type_2_node node  { };
			Node_index  index { INVALID_NODE_INDEX };
		};

		struct Local_cache_request
		{
			enum State { INVALID, PENDING, IN_PROGRESS, COMPLETE };
			enum Op { READ, WRITE, SYNC };

			State            state                  { INVALID };
			Op               op                     { READ };
			bool             success                { false };
			Genode::uint64_t pba                    { 0 };
			Genode::uint64_t level                  { 0 };
		};

		struct Local_meta_tree_request
		{
			enum State { INVALID, PENDING, IN_PROGRESS, COMPLETE };
			enum Op { READ, WRITE, SYNC };

			State            state { INVALID };
			Op               op    { READ };
			Genode::uint64_t pba   { 0 };
		};

		class Type_1_info_stack {

			private:

				enum { MIN = 1, MAX = TREE_MAX_DEGREE,  };

				Type_1_info _container[MAX + 1] { };
				uint64_t    _top                { MIN - 1 };

			public:

				bool empty() const { return _top < MIN; }

				bool full() const { return _top >= MAX; }

				Type_1_info peek_top() const
				{
					if (empty()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					return _container[_top];
				}

				void reset() { _top = MIN - 1; }

				void pop()
				{
					if (empty()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					_top--;
				}

				void push(Type_1_info val)
				{
					if (full()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					_top++;
					_container[_top] = val;
				}

				void update_top(Type_1_info val)
				{
					if (empty()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					_container[_top] = val;
				}
		};

		class Type_2_info_stack {

			private:

				enum { MIN = 1, MAX = TREE_MAX_DEGREE,  };

				Type_2_info _container[MAX + 1] { };
				uint64_t    _top                { MIN - 1 };

			public:

				bool empty() const { return _top < MIN; }

				bool full() const { return _top >= MAX; }

				Type_2_info peek_top() const
				{
					if (empty()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					return _container[_top];
				}

				void reset() { _top = MIN - 1; }

				void pop()
				{
					if (empty()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					_top--;
				}

				void push(Type_2_info val)
				{
					if (full()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					_top++;
					_container[_top] = val;
				}

				void update_top(Type_2_info val)
				{
					if (empty()) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					_container[_top] = val;
				}
		};

		class Node_queue
		{
			private:

				enum {
					FIRST_CONTAINER_IDX = 1,
					LAST_CONTAINER_IDX = TREE_MAX_DEGREE,
					LAST_USED_VALUE = TREE_MAX_DEGREE - 1,
					FIRST_USED_VALUE = 0,
				};

				Genode::uint64_t _head                              { FIRST_CONTAINER_IDX };
				Genode::uint64_t _tail                              { FIRST_CONTAINER_IDX };
				Type_2_info      _container[LAST_CONTAINER_IDX + 1] { };
				Genode::uint64_t _used                              { FIRST_USED_VALUE };

			public:

				void enqueue(Type_2_info const &node)
				{
					_container[_tail] = node;
					if (_tail < LAST_CONTAINER_IDX)
						_tail++;
					else
						_tail = FIRST_CONTAINER_IDX;

					_used++;
				}

				void dequeue_head()
				{
					if (_head < LAST_CONTAINER_IDX)
						_head++;
					else
						_head = FIRST_CONTAINER_IDX;

					_used--;
				}

				Type_2_info const &head() const { return _container[_head]; }

				bool empty() const { return _used == FIRST_USED_VALUE; };

				bool full() const { return _used == LAST_USED_VALUE; };
		};

		State                   _state                                 { INVALID };
		Request                 _request                               { };
		Genode::uint64_t        _needed_blocks                         { 0 };
		Genode::uint64_t        _found_blocks                          { 0 };
		Genode::uint64_t        _exchanged_blocks                      { 0 };
		Local_meta_tree_request _meta_tree_request                     { };
		Local_cache_request     _cache_request                         { };
		Block_data              _cache_block_data                      { };
		Type_1_info_stack       _level_n_stacks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_info_stack       _level_0_stack                         { };
		Type_1_node_block       _level_n_nodes[TREE_MAX_NR_OF_LEVELS]  { };
		Type_1_node_block       _level_n_node                          { };
		Type_2_node_block       _level_0_node                          { };
		Node_queue              _type_2_leafs                          { };
		Genode::uint64_t        _vbd_degree_log_2                      { 0 };
		bool                    _wb_data_prim_success                  { false };

		Type_1_node _root_node() const
		{
			Type_1_node node { };
			node.pba = *(Physical_block_address *)_request._ft_root_pba_ptr;
			node.gen = *(Generation *)_request._ft_root_gen_ptr;
			memcpy(&node.hash, (void *)_request._ft_root_hash_ptr, HASH_SIZE);
			return node;
		}
};

class Cbe::Free_tree : public Module
{
	private:

		using Request = Free_tree_request;
		using Channel = Free_tree_channel;
		using Local_cache_request = Channel::Local_cache_request;
		using Local_meta_tree_request = Channel::Local_meta_tree_request;
		using Type_1_info = Channel::Type_1_info;
		using Type_2_info = Channel::Type_2_info;
		using Type_1_info_stack = Channel::Type_1_info_stack;
		using Type_2_info_stack = Channel::Type_2_info_stack;
		using Node_queue = Channel::Node_queue;

		enum { FIRST_LVL_N_STACKS_IDX = 1 };
		enum { LAST_LVL_N_STACKS_IDX = TREE_MAX_LEVEL };
		enum { FIRST_LVL_N_NODES_IDX = 1 };
		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _reset_block_state(Channel &chan);

		static Local_meta_tree_request
		_new_meta_tree_request(Physical_block_address pba);

		void _update_upper_n_stack(Type_1_info const &t,
		                           Generation         gen,
		                           Block_data  const &block_data,
		                           Type_1_node_block &entries);

		void
		_exchange_type_2_leaves(Generation              free_gen,
		                        Tree_level_index        max_level,
		                        Type_1_node_walk const &old_blocks,
		                        Tree_walk_pbas         &new_blocks,
		                        Virtual_block_address   vba,
		                        Tree_degree_log_2       vbd_degree_log_2,
		                        Request::Type           req_type,
		                        Type_2_info_stack      &stack,
		                        Type_2_node_block      &entries,
		                        Number_of_blocks       &exchanged,
		                        bool                   &handled,
		                        Virtual_block_address   vbd_highest_vba,
		                        bool                    rekeying,
		                        Key_id                  previous_key_id,
		                        Key_id                  current_key_id,
		                        Virtual_block_address   rekeying_vba);

		void _populate_lower_n_stack(Type_1_info_stack &stack,
		                             Type_1_node_block &entries,
		                             Block_data const  &block_data,
		                             Generation         current_gen);

		bool
		_check_type_2_leaf_usable(Snapshots       const &snapshots,
		                          Generation             last_secured_gen,
		                          Type_2_node     const &node,
		                          bool                   rekeying,
		                          Key_id                 previous_key_id,
		                          Virtual_block_address  rekeying_vba);

		void _populate_level_0_stack(Type_2_info_stack     &stack,
		                             Type_2_node_block     &entries,
		                             Block_data      const &block_data,
		                             Snapshots       const &active_snaps,
		                             Generation             secured_gen,
		                             bool                   rekeying,
		                             Key_id                 previous_key_id,
		                             Virtual_block_address  rekeying_vba);

		void _execute_update(Channel         &chan,
		                     Snapshots const &active_snaps,
		                     Generation       last_secured_gen,
		                     bool            &progress);

		bool _node_volatile(Type_1_node const &node,
		                    Genode::uint64_t   gen);

		void _execute_scan(Channel         &chan,
		                   Snapshots const &active_snaps,
		                   Generation       last_secured_gen,
		                   bool            &progress);

		void _execute(Channel         &chan,
		              Snapshots const &active_snaps,
		              Generation       last_secured_gen,
		              bool            &progress);

		void _check_type_2_stack(Type_2_info_stack &stack,
		                         Type_1_info_stack &stack_next,
		                         Node_queue        &leaves,
		                         Number_of_blocks  &found);

		Local_cache_request _new_cache_request(Physical_block_address  pba,
		                                       Local_cache_request::Op op,
		                                       Tree_level_index        lvl);

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;
};

#endif /* _FREE_TREE_H_ */
