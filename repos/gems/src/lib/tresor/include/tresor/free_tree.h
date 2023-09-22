/*
 * \brief  Module for doing VBD COW allocations on the free tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FREE_TREE_H_
#define _TRESOR__FREE_TREE_H_

/* base includes */
#include <util/reconstructible.h>

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>

namespace Tresor {

	class Free_tree;
	class Free_tree_request;
	class Free_tree_channel;
}

class Tresor::Free_tree_request : public Module_request
{
	public:

		enum Type { ALLOC_FOR_NON_RKG, ALLOC_FOR_RKG_CURR_GEN_BLKS, ALLOC_FOR_RKG_OLD_GEN_BLKS };

	private:

		friend class Free_tree;
		friend class Free_tree_channel;

		Type _type;
		Free_tree_root &_ft;
		Meta_tree_root &_mt;
		Generation _curr_gen;
		Generation _free_gen;
		Number_of_blocks _num_requested_blks;
		Tree_walk_pbas &_new_blocks;
		Type_1_node_walk const &_old_blocks;
		Tree_level_index _max_lvl;
		Virtual_block_address _vba;
		Tree_degree _vbd_degree;
		Virtual_block_address _vbd_highest_vba;
		bool _rekeying;
		Key_id _prev_key_id;
		Key_id _curr_key_id;
		Virtual_block_address _rekeying_vba;
		bool &_success;
		Snapshots const &_snapshots;
		Generation _last_secured_gen;

		NONCOPYABLE(Free_tree_request);

	public:

		Free_tree_request(Module_id src_module_id,
		                  Module_request_id src_request_id,
		                  Type type,
		                  Free_tree_root &ft,
		                  Meta_tree_root &mt,
		                  Snapshots const &snapshots,
		                  Generation last_secured_gen,
		                  Generation curr_gen,
		                  Generation free_gen,
		                  Number_of_blocks num_requested_blks,
		                  Tree_walk_pbas &new_blocks,
		                  Type_1_node_walk const &old_blocks,
		                  Tree_level_index max_lvl,
		                  Virtual_block_address vba,
		                  Tree_degree vbd_degree,
		                  Virtual_block_address vbd_highest_vba,
		                  bool rekeying,
		                  Key_id prev_key_id,
		                  Key_id curr_key_id,
		                  Virtual_block_address rekeying_vba,
		                  bool &success);

		static char const *type_to_string(Type type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Tresor::Free_tree_channel : public Module_channel
{
	private:

		friend class Free_tree;

		using Request = Free_tree_request;

		enum State {
			SCAN,
			SCAN_COMPLETE,
			UPDATE,
			UPDATE_COMPLETE,
			COMPLETE,
			NOT_ENOUGH_FREE_BLOCKS,
			TREE_HASH_MISMATCH
		};

		enum Gen_req_state {
			REQ_INVALID,
			REQ_IN_PROGRESS,
			READ_COMPLETE,
			WRITE_COMPLETE,
			ALLOC_COMPLETE
		};

		struct Type_1_info
		{
			enum State {
				INVALID, AVAILABLE, READ, WRITE, COMPLETE };

			State           state   { INVALID };
			Type_1_node     node    { };
			Tree_node_index index   { INVALID_NODE_INDEX };
			bool            volatil { false };
		};

		struct Type_2_info
		{
			enum State {
				INVALID, AVAILABLE, READ, WRITE, COMPLETE };

			State           state { INVALID };
			Type_2_node     node  { };
			Tree_node_index index { INVALID_NODE_INDEX };
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
					MAX_CONTAINER_IDX = TREE_MAX_DEGREE,
					MAX_USED_VALUE = TREE_MAX_DEGREE - 1,
					FIRST_USED_VALUE = 0,
				};

				uint64_t    _head                             { FIRST_CONTAINER_IDX };
				uint64_t    _tail                             { FIRST_CONTAINER_IDX };
				Type_2_info _container[MAX_CONTAINER_IDX + 1] { };
				uint64_t    _used                             { FIRST_USED_VALUE };

			public:

				void enqueue(Type_2_info const &node)
				{
					_container[_tail] = node;
					if (_tail < MAX_CONTAINER_IDX)
						_tail++;
					else
						_tail = FIRST_CONTAINER_IDX;

					_used++;
				}

				void dequeue_head()
				{
					if (_head < MAX_CONTAINER_IDX)
						_head++;
					else
						_head = FIRST_CONTAINER_IDX;

					_used--;
				}

				Type_2_info const &head() const { return _container[_head]; }

				bool empty() const { return _used == FIRST_USED_VALUE; };

				bool full() const { return _used == MAX_USED_VALUE; };
		};

		State _state { COMPLETE };
		Request *_req_ptr { nullptr };
		uint64_t _needed_blocks { 0 };
		uint64_t _found_blocks { 0 };
		uint64_t _exchanged_blocks { 0 };
		Block _cache_block_data { };
		Type_1_info_stack _level_n_stacks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_info_stack _level_0_stack { };
		Type_1_node_block _level_n_nodes[TREE_MAX_NR_OF_LEVELS]  { };
		Type_1_node_block _level_n_node { };
		Type_2_node_block _level_0_node { };
		Node_queue _type_2_leafs { };
		Tree_degree_log_2 _vbd_degree_log_2 { 0 };
		bool _wb_data_prim_success { false };
		Gen_req_state _generated_req_state { REQ_INVALID };
		Tree_level_index _generated_req_lvl { 0 };
		Physical_block_address _generated_req_pba { 0 };
		bool _generated_req_success { false };

		Type_1_node _root_node() const
		{
			Type_1_node node { };
			node.pba = _req_ptr->_ft.pba;
			node.gen = _req_ptr->_ft.gen;
			node.hash = _req_ptr->_ft.hash;
			return node;
		}

		NONCOPYABLE(Free_tree_channel);

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_cache_req(State_uint state, bool &progress, Tree_level_index lvl, ARGS &&... args)
		{
			ASSERT(_generated_req_state == REQ_INVALID);
			_generated_req_lvl = lvl;
			_generated_req_state = REQ_IN_PROGRESS;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _generate_mt_req(State_uint state, bool &progress, Physical_block_address pba);

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == COMPLETE; }

	public:

		Free_tree_channel(Module_channel_id id) : Module_channel { FREE_TREE, id } { }
};

class Tresor::Free_tree : public Module
{
	private:

		using Request = Free_tree_request;
		using Channel = Free_tree_channel;
		using Type_1_info = Channel::Type_1_info;
		using Type_2_info = Channel::Type_2_info;
		using Type_1_info_stack = Channel::Type_1_info_stack;
		using Type_2_info_stack = Channel::Type_2_info_stack;
		using Node_queue = Channel::Node_queue;

		enum { FIRST_LVL_N_STACKS_IDX = 1 };
		enum { MAX_LVL_N_STACKS_IDX = TREE_MAX_LEVEL };
		enum { FIRST_LVL_N_NODES_IDX = 1 };

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Free_tree);

		void _update_upper_n_stack(Type_1_info const &t,
		                           Generation         gen,
		                           Block       const &block_data,
		                           Type_1_node_block &entries);

		void _mark_req_failed(Channel    &chan,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &chan,
		                          bool    &progress);

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
		                             Block      const  &block_data,
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
		                             Block           const &block_data,
		                             Snapshots       const &active_snaps,
		                             Generation             secured_gen,
		                             bool                   rekeying,
		                             Key_id                 previous_key_id,
		                             Virtual_block_address  rekeying_vba);

		void _execute_update(Channel         &chan,
		                     Snapshots const &active_snaps,
		                     Generation       last_secured_gen,
		                     bool            &progress);

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

		void execute(bool &) override;

	public:

		Free_tree();
};

#endif /* _TRESOR__FREE_TREE_H_ */
