/*
 * \brief  Module for doing VBD COW allocations on the meta tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__META_TREE_H_
#define _TRESOR__META_TREE_H_

/* base includes */
#include <util/reconstructible.h>

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>

namespace Tresor {

	class Meta_tree;
	class Meta_tree_request;
	class Meta_tree_channel;
}

class Tresor::Meta_tree_request : public Module_request
{
	friend class Meta_tree_channel;

	public:

		enum Type { ALLOC_PBA };

	private:

		Type const _type;
		Meta_tree_root &_mt;
		Generation const _curr_gen;
		Physical_block_address &_pba;
		bool &_success;

		NONCOPYABLE(Meta_tree_request);

	public:

		Meta_tree_request(Module_id, Module_channel_id, Type, Meta_tree_root &, Generation,
		                  Physical_block_address &, bool &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Tresor::Meta_tree_channel : public Module_channel
{
	private:

		using Request = Meta_tree_request;

		enum State { REQ_SUBMITTED, REQ_GENERATED, SEEK_DOWN, SEEK_LEFT_OR_UP, WRITE_BLK, COMPLETE };

		State _state { COMPLETE };
		Request *_req_ptr { nullptr };
		bool _pba_allocated { false };
		Block _blk { };
		Tree_node_index _node_idx[TREE_MAX_NR_OF_LEVELS] { };
		Type_1_node_block _t1_blks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_node_block _t2_blk { };
		Tree_level_index _lvl { 0 };
		bool _generated_req_success { false };

		NONCOPYABLE(Meta_tree_channel);

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _mark_req_failed(bool &, char const *);

		bool _can_alloc_pba_of(Type_2_node &);

		void _alloc_pba_of(Type_2_node &, Physical_block_address &);

		void _traverse_curr_node(bool &);

		void _mark_req_successful(bool &);

		void _start_tree_traversal(bool &);

		void _advance_to_next_node();

	public:

		Meta_tree_channel(Module_channel_id id) : Module_channel { META_TREE, id } { }

		void execute(bool &);
};

class Tresor::Meta_tree : public Module
{
	private:

		using Channel = Meta_tree_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Meta_tree);

		void execute(bool &) override;

	public:

		struct Alloc_pba : Meta_tree_request
		{
			Alloc_pba(Module_id m, Module_channel_id c, Meta_tree_root &t, Generation g, Physical_block_address &a, bool &s)
			: Meta_tree_request(m, c, Meta_tree_request::ALLOC_PBA, t, g, a, s) { }
		};

		Meta_tree();
};

#endif /* _TRESOR__META_TREE_H_ */
