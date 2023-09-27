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

		Free_tree_request(Module_id src_module_id,
		                  Module_request_id src_request_id,
		                  Type type,
		                  Free_tree_root &ft,
		                  Meta_tree_root &mt,
		                  Snapshots const &snapshots,
		                  Generation last_secured_gen,
		                  Generation curr_gen,
		                  Generation free_gen,
		                  Number_of_blocks num_required_pbas,
		                  Tree_walk_pbas &new_blocks,
		                  Type_1_node_walk const &old_blocks,
		                  Tree_level_index max_lvl,
		                  Virtual_block_address vba,
		                  Tree_degree vbd_degree,
		                  Virtual_block_address vbd_max_vba,
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

		using Request = Free_tree_request;

		enum { FIRST_LVL_N_STACKS_IDX = 1 };
		enum { MAX_LVL_N_STACKS_IDX = TREE_MAX_LEVEL };
		enum { FIRST_LVL_N_NODES_IDX = 1 };

		enum State {
			REQ_SUBMITTED,
			REQ_GENERATED,
			SCAN_READ_BLK_SUCCEEDED,
			UPDATE_REQ_INVALID,
			UPDATE_REQ_GENERATED,
			UPDATE_READ_BLK_SUCCEEDED,
			UPDATE_ALLOC_PBA_SUCCEEDED,
			UPDATE_WRITE_BLK_SUCCEEDED,
			COMPLETE
		};

		enum Type_1_info_state {
			SUBTREE_NOT_TRAVERSED, SUBTREE_ROOT_BLK_READ, X_WRITE, SUBTREE_TRAVERSED };

		struct Type_1_info
		{
			Type_1_info_state state { SUBTREE_NOT_TRAVERSED };
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

		class Type_1_info_stack
		{
			private:

				enum { MIN = 1, MAX = TREE_MAX_DEGREE,  };

				Type_1_info _container[MAX + 1] { };
				uint64_t    _top                { MIN - 1 };

			public:

				bool empty() const { return _top < MIN; }

				bool full() const { return _top >= MAX; }

				Type_1_info &top()
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
		};

		class Type_2_info_stack
		{
			private:

				enum { MIN = 1, MAX = TREE_MAX_DEGREE,  };

				Type_2_info _container[MAX + 1] { };
				uint64_t    _top                { MIN - 1 };

			public:

				bool empty() const { return _top < MIN; }

				bool full() const { return _top >= MAX; }

				Type_2_info &top()
				{
					ASSERT(!empty());
					return _container[_top];
				}

				void reset() { _top = MIN - 1; }

				void pop()
				{
					ASSERT(!empty());
					_top--;
				}

				void push(Type_2_info val)
				{
					ASSERT(!full());
					_top++;
					_container[_top] = val;
				}
		};

		State _state { COMPLETE };
		Request *_req_ptr { nullptr };
		Number_of_blocks _found_blocks { 0 };
		Number_of_blocks _num_allocated_pbas { 0 };
		Block _cache_block_data { };
		Type_1_info_stack _level_n_stacks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_info_stack _level_0_stack { };
		Type_1_node_block _level_n_nodes[TREE_MAX_NR_OF_LEVELS]  { };
		Type_2_node_block _level_0_node { };
		Tree_degree_log_2 _vbd_degree_log_2 { 0 };
		bool _wb_data_prim_success { false };
		Tree_level_index _lvl { 0 };
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
			switch (_state) {
			case REQ_GENERATED: ASSERT_NEVER_REACHED;
			case SCAN_READ_BLK_SUCCEEDED:
				_lvl = lvl;
				_state = REQ_GENERATED;
				generate_req<REQUEST>(state, progress, args..., _generated_req_success);
				break;
			default:
				ASSERT(_state == UPDATE_REQ_INVALID);
				_lvl = lvl;
				_state = UPDATE_REQ_GENERATED;
				generate_req<REQUEST>(state, progress, args..., _generated_req_success);
				break;
			}
		}

		void _generate_mt_req(State_uint state, bool &progress, Physical_block_address pba);

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _mark_req_failed(bool &, char const *);

		Tree_level_index _lowest_non_empty_lvl() const;

		bool _t2_node_allocable(Type_2_node &node);

		void _init_info_stack_from_blk_data(Tree_level_index);

		void _traverse_tree(bool &);

		void _try_alloc_pbas_from_lvl_0_stack();

		void _update_t1_node(Type_1_node &, Type_1_info &);

		void _mark_req_successful(bool &);

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
