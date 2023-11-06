/*
 * \brief  Module for initializing the FT
 * \author Josef Soentgen
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FT_INITIALIZER_H_
#define _TRESOR__FT_INITIALIZER_H_

/* base includes */
#include <base/output.h>

/* tresor includes */
#include <tresor/module.h>

namespace Tresor {

	class Ft_initializer;
	class Ft_initializer_request;
	class Ft_initializer_channel;
}


class Tresor::Ft_initializer_request : public Module_request
{
	friend class Ft_initializer_channel;

	private:

		Free_tree_root &_ft;
		Pba_allocator &_pba_alloc;
		bool &_success;

		NONCOPYABLE(Ft_initializer_request);

	public:

		Ft_initializer_request(Module_id, Module_channel_id, Free_tree_root &, Pba_allocator &, bool &);

		void print(Output &out) const override { Genode::print(out, "init"); }
};


class Tresor::Ft_initializer_channel : public Module_channel
{
	private:

		using Request = Ft_initializer_request;

		enum State { REQ_GENERATED, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE, BLOCK_IO_COMPLETE };

		enum Node_state { DONE, INIT_BLOCK, INIT_NODE, WRITE_BLOCK };

		struct Type_1_level
		{
			Type_1_node_block children { };
			Node_state children_state[NR_OF_T1_NODES_PER_BLK] { DONE };
		};

		struct Type_2_level
		{
			Type_2_node_block children { };
			Node_state children_state[NR_OF_T2_NODES_PER_BLK] { DONE };
		};

		State _state { COMPLETE };
		Request *_req_ptr { };
		Type_1_level _t1_levels[TREE_MAX_NR_OF_LEVELS] { };
		Type_2_level _t2_level { };
		Tree_level_index _level_to_write { 0 };
		Physical_block_address _pba { 0 };
		Number_of_leaves _num_remaining_leaves { 0 };
		bool _generated_req_success { false };
		Block _blk { };

		NONCOPYABLE(Ft_initializer_channel);

		void _reset_level(Tree_level_index lvl, Node_state node_state)
		{
			if (lvl == 1) {
				for (Tree_node_index idx = 0; idx < NR_OF_T2_NODES_PER_BLK; idx++) {
					_t2_level.children.nodes[idx] = { };
					_t2_level.children_state[idx] = node_state;
				}
			} else {
				for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {
					_t1_levels[lvl].children.nodes[idx] = { };
					_t1_levels[lvl].children_state[idx] = node_state;
				}
			}
		}

		void _generate_blk_io_write(bool &progress);

		void _generated_req_completed(State_uint) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _request_submitted(Module_request &) override;

		void _execute_t2_node(Tree_node_index, bool &);

		void _execute_t1_node(Tree_level_index, Tree_node_index, bool &);

		void _mark_req_failed(bool &, char const *);

		void _mark_req_successful(bool &);

		void _execute(bool &);

		void _execute_init(bool &);

	public:

		Ft_initializer_channel(Module_channel_id id) : Module_channel { FT_INITIALIZER, id } { }

		void execute(bool &);
};


class Tresor::Ft_initializer : public Module
{
	private:

		using Channel = Ft_initializer_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Ft_initializer);

	public:

		Ft_initializer();

		void execute(bool &) override;

};

#endif /* _TRESOR__FT_INITIALIZER_H_ */
