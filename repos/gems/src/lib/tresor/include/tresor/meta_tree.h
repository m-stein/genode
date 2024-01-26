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

/* tresor includes */
#include <tresor/types.h>
#include <tresor/block_io.h>

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
		Tree_root &_mt;
		Generation const _curr_gen;
		Physical_block_address &_pba;
		bool &_success;

		NONCOPYABLE(Meta_tree_request);

	public:

		Meta_tree_request(Module_id, Module_channel_id, Type, Tree_root &, Generation, Physical_block_address &, bool &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Meta_tree_channel : public Module_channel
{
	public:

		using Module = Meta_tree;

		struct Attr { };

	private:

		using Request = Meta_tree_request;

		enum State {
			REQ_SUBMITTED, REQ_GENERATED, READ_BLK, SEEK_DOWN, SEEK_LEFT_OR_UP, WRITE_BLK, WRITE_BLK_SUCCEEDED, COMPLETE, INIT };

		using Helper = Request_helper<Meta_tree_channel, State>;

		Helper _helper;
		Request *_req_ptr { nullptr };
		Block _blk { };
		Tree_node_index _node_idx[TREE_MAX_NR_OF_LEVELS] { };
		Type_1_node_block _t1_blks[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_node_block _t2_blk { };
		Tree_level_index _lvl { 0 };
		union {
			Generated_request<Helper, Block_io::Read, State> _read_block;
			Generated_request<Helper, Block_io::Write, State> _write_block;
		};

		NONCOPYABLE(Meta_tree_channel);

		void _generated_req_completed(State_uint) override { };

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _helper.state == COMPLETE; }

		void _mark_req_failed(bool &, char const *);

		bool _can_alloc_pba_of(Type_2_node &);

		void _alloc_pba_of(Type_2_node &, Physical_block_address &);

		void _traverse_curr_node(bool &);

		void _mark_req_successful(bool &);

		void _start_tree_traversal(bool &);

	public:

		Meta_tree_channel(Module_channel_id id) : Module_channel(META_TREE, id), _helper(*this, Attr()) { }

		~Meta_tree_channel() { }

		void execute(bool &, Block_io &);

		void print(Output &out) const { ASSERT(_req_ptr); Genode::print(out, *_req_ptr); }
};

class Tresor::Meta_tree : public Module
{
	private:

		using Channel = Meta_tree_channel;

		Constructible<Channel> _channels[1] { };
		Block_io &_block_io;

		NONCOPYABLE(Meta_tree);

		void execute(bool &) override;

	public:

		struct Alloc_pba : Meta_tree_request
		{
			Alloc_pba(Module_id src_mod, Module_channel_id src_chan, Tree_root &mt, Generation gen, Physical_block_address &pba, bool &succ)
			: Meta_tree_request(src_mod, src_chan, Meta_tree_request::ALLOC_PBA, mt, gen, pba, succ) { }
		};

		Meta_tree(Block_io &block_io);

		static constexpr char const *name() { return "meta_tree"; }
};

#endif /* _TRESOR__META_TREE_H_ */
