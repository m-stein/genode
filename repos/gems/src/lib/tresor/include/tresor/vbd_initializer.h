/*
 * \brief  Module for initializing the VBD
 * \author Josef Soentgen
 * \date   2023-03-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VBD_INITIALIZER_H_
#define _TRESOR__VBD_INITIALIZER_H_

/* tresor includes */
#include <tresor/module.h>

namespace Tresor {

	class Vbd_initializer;
	class Vbd_initializer_request;
	class Vbd_initializer_channel;
}


class Tresor::Vbd_initializer_request : public Module_request
{
	friend class Vbd_initializer;
	friend class Vbd_initializer_channel;

	private:

		Tree_root &_vbd;
		Pba_allocator &_pba_alloc;
		bool &_success;

	public:

		Vbd_initializer_request(Module_id, Module_request_id, Tree_root &, Pba_allocator &, bool &);

		void print(Output &out) const override { Genode::print(out, "init"); }
};


class Tresor::Vbd_initializer_channel : public Module_channel
{
	friend class Vbd_initializer;

	private:

		using Request = Vbd_initializer_request;

		enum State { REQ_GENERATED, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE, BLOCK_IO_COMPLETE };

		enum Child_state { DONE, INIT_BLOCK, INIT_NODE, WRITE_BLOCK, };

		struct Type_1_level
		{
			Type_1_node_block children { };
			Child_state       children_state[NR_OF_T1_NODES_PER_BLK] { DONE };
		};

		struct Root_node
		{
			Type_1_node node  { };
			Child_state state { DONE };
		};

		State _state { COMPLETE };
		Vbd_initializer_request *_req_ptr { };
		Root_node _root_node { };
		Type_1_level _t1_levels[TREE_MAX_LEVEL] { };
		uint64_t _level_to_write { 0 };
		uint64_t _child_pba { 0 };
		bool _generated_req_success { false };
		Block _encoded_blk { };
		Hash _dummy_hash { };
		Number_of_leaves _num_remaining_leaves { };

		void _generated_req_completed(State_uint) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _request_submitted(Module_request &) override;

		void _generate_blk_io_write(bool &progress);

		static void reset_node(Type_1_node &node)
		{
			memset(&node, 0, sizeof(Type_1_node));
		}

		static void reset_level(Type_1_level &level,
		                        Child_state   state)
		{
			for (unsigned int i = 0; i < NR_OF_T1_NODES_PER_BLK; i++) {
				reset_node(level.children.nodes[i]);
				level.children_state[i] = state;
			}
		}

		static void dump(Type_1_node_block const &node_block)
		{
			for (auto v : node_block.nodes) {
				log(v);
			}
		}
};


class Tresor::Vbd_initializer : public Module
{
	private:

		using Request = Vbd_initializer_request;
		using Channel = Vbd_initializer_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _execute_leaf_child(Channel                              &channel,
		                         bool                                 &progress,
		                         uint64_t                             &nr_of_leaves,
		                         Type_1_node                          &child,
		                         Vbd_initializer_channel::Child_state &child_state,
		                         uint64_t                              level_index,
		                         uint64_t                              child_index);

		void _execute_inner_t1_child(Channel                               &channel,
		                             bool                                  &progress,
		                             uint64_t                               nr_of_leaves,
		                             uint64_t                              &level_to_write,
		                             Type_1_node                           &child,
		                             Vbd_initializer_channel::Type_1_level &child_level,
		                             Vbd_initializer_channel::Child_state  &child_state,
		                             uint64_t                               level_index,
		                             uint64_t                               child_index);

		void _execute(Channel &channel,
		              bool    &progress);

		void _execute_init(Channel &channel,
		                   bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);

	public:

		Vbd_initializer();

		void execute(bool &) override;

};

#endif /* _TRESOR__VBD_INITIALIZER_H_ */
