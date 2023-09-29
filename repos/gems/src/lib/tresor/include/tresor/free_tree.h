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
	friend class Free_tree_channel;

	public:

		enum Type { ALLOC_FOR_NON_RKG, ALLOC_FOR_RKG_CURR_GEN_BLKS, ALLOC_FOR_RKG_OLD_GEN_BLKS };

	private:

		Type const _type;
		Free_tree_root &_ft;
		Meta_tree_root &_mt;
		Generation const _curr_gen;
		Generation const _free_gen;
		Number_of_blocks const _num_required_pbas;
		Tree_walk_pbas &_new_blocks;
		Type_1_node_walk const &_old_blocks;
		Tree_level_index const _max_lvl;
		Virtual_block_address const _vba;
		Tree_degree const _vbd_degree;
		Virtual_block_address const _vbd_max_vba;
		bool const _rekeying;
		Key_id const _prev_key_id;
		Key_id const _curr_key_id;
		Virtual_block_address const _rekeying_vba;
		bool &_success;
		Snapshots const &_snapshots;
		Generation const _last_secured_gen;

		NONCOPYABLE(Free_tree_request);

	public:

		Free_tree_request(Module_id, Module_request_id, Type, Free_tree_root &, Meta_tree_root &, Snapshots const &,
		                  Generation, Generation, Generation, Number_of_blocks, Tree_walk_pbas &, Type_1_node_walk const &,
		                  Tree_level_index, Virtual_block_address, Tree_degree, Virtual_block_address,
		                  bool, Key_id, Key_id, Virtual_block_address, bool &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Tresor::Free_tree_channel : public Module_channel
{
	private:

		using Request = Free_tree_request;

		enum State {
			REQ_SUBMITTED, REQ_GENERATED, READ_BLK_SUCCEEDED, ALLOC_PBA_SUCCEEDED, WRITE_BLK_SUCCEEDED, COMPLETE };

		enum Node_info_state {
			SUBTREE_NOT_TRAVERSED,
			SUBTREE_ROOT_BLK_READ,
			SUBTREE_MODIFIED,
			SUBTREE_ROOT_BLK_READY_FOR_WRITE,
			SUBTREE_TRAVERSED };

		template <typename NODE>
		struct Node_info
		{
			Tree_node_index index { INVALID_NODE_INDEX };
		};

		template <typename T>
		class Node_info_stack
		{
			private:

				using Index = uint64_t;

				T _slots[TREE_MAX_DEGREE + 1] { };
				Index _top_idx { 0 };

				NONCOPYABLE(Node_info_stack);

			public:

				Node_info_stack() { }

				bool empty() const { return !_top_idx; }

				T &top()
				{
					ASSERT(!empty());
					return _slots[_top_idx];
				}

				void reset() { _top_idx = 0; }

				Number_of_blocks num_items() { return _top_idx; }

				void pop()
				{
					ASSERT(!empty());
					_top_idx--;
				}

				void push(T obj)
				{
					ASSERT(_top_idx < TREE_MAX_DEGREE);
					_top_idx++;
					_slots[_top_idx] = obj;
				}
		};

		using Type_1_info = Node_info<Type_1_node>;
		using Type_2_info = Node_info<Type_2_node>;
		using Type_1_info_stack = Node_info_stack<Type_1_info>;
		using Type_2_info_stack = Node_info_stack<Type_2_info>;

		State _state { COMPLETE };
		Request *_req_ptr { nullptr };
		Number_of_blocks _num_pbas { 0 };
		Block _blk { };
		Node_info_state _node_state[TREE_MAX_NR_OF_LEVELS] { };
		bool _alloc_pbas { false };
		Type_1_info_stack _t1_info_stacks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_info_stack _t2_info_stack { };
		Type_1_node_block _t1_blks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_node_block _t2_blk { };
		Tree_degree_log_2 _vbd_degree_log_2 { 0 };
		Tree_level_index _lvl { 0 };
		bool _generated_req_success { false };

		NONCOPYABLE(Free_tree_channel);

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_cache_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _generate_mt_req(State_uint state, bool &progress, Physical_block_address &pba);

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _mark_req_failed(bool &, char const *);

		bool _t2_node_allocable(Type_2_node &node);

		void _init_stack_from_blk(Tree_level_index);

		void _traverse_tree(bool &progress);

		void _alloc_pbas_from_t2_info_stack();

		void _mark_req_successful(bool &);

		bool _info_stack_empty(Tree_level_index lvl) const { return lvl ? _t1_info_stacks[lvl].empty() : _t2_info_stack.empty(); }

		void _start_tree_traversal(bool &progress);

	public:

		Free_tree_channel(Module_channel_id id) : Module_channel { FREE_TREE, id } { }

		void execute(bool &);
};

class Tresor::Free_tree : public Module
{
	private:

		using Channel = Free_tree_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Free_tree);

		void execute(bool &) override;

	public:

		Free_tree();
};

#endif /* _TRESOR__FREE_TREE_H_ */
