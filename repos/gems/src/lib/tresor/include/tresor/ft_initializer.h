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
	friend class Ft_initializer;
	friend class Ft_initializer_channel;

	private:

		Free_tree_root &_ft;
		Pba_allocator &_pba_alloc;
		bool &_success;

		NONCOPYABLE(Ft_initializer_request)

	public:

		Ft_initializer_request(Module_id, Module_channel_id, Free_tree_root &, Pba_allocator &, bool &);

		void print(Output &out) const override { Genode::print(out, "init"); }
};


class Tresor::Ft_initializer_channel : public Module_channel
{
	friend class Ft_initializer;

	private:

		using Request = Ft_initializer_request;

		enum State { INACTIVE, REQ_GENERATED, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE, BLOCK_IO_COMPLETE };

		enum Child_state { DONE, INIT_BLOCK, INIT_NODE, WRITE_BLOCK, };

		struct Type_1_level
		{
			Type_1_node_block children { };
			Child_state       children_state[NR_OF_T1_NODES_PER_BLK] { DONE };
		};

		struct Type_2_level
		{
			Type_2_node_block children { };
			Child_state       children_state[NR_OF_T2_NODES_PER_BLK] { DONE };
		};

		struct Root_node
		{
			Type_1_node node  { };
			Child_state state { DONE };
		};

		State _state { INACTIVE };
		Constructible<Ft_initializer_request> _req_ptr { };
		Root_node _root_node { };
		Type_1_level _t1_levels[TREE_MAX_LEVEL] { };
		Type_2_level _t2_level { };
		uint64_t _level_to_write { 0 };
		uint64_t _child_pba { 0 };
		Number_of_leaves _num_remaining_leaves { 0 };
		bool _generated_req_success { false };
		Block _encoded_blk { };

		static void reset_node(Tresor::Type_1_node &node)
		{
			memset(&node, 0, sizeof(Type_1_node));
		}

		static void reset_node(Tresor::Type_2_node &node)
		{
			memset(&node, 0, sizeof(Type_2_node));
		}

		static void reset_level(Type_1_level &level,
		                        Child_state   state)
		{
			for (unsigned int i = 0; i < NR_OF_T1_NODES_PER_BLK; i++) {
				reset_node(level.children.nodes[i]);
				level.children_state[i] = state;
			}
		}

		static void reset_level(Type_2_level &level,
		                        Child_state   state)
		{
			for (unsigned int i = 0; i < NR_OF_T2_NODES_PER_BLK; i++) {
				reset_node(level.children.nodes[i]);
				level.children_state[i] = state;
			}
		}

		void _generate_blk_io_write(bool &progress);

		void _generated_req_completed(State_uint) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _request_submitted(Module_request &) override { ASSERT_NEVER_REACHED; }

		void _execute_leaf_child(bool                                &progress,
		                         uint64_t                            &nr_of_leaves,
		                         Tresor::Type_2_node                 &child,
		                         Ft_initializer_channel::Child_state &child_state,
		                         uint64_t                             child_index);

		void _execute_inner_t2_child(bool                                 &progress,
		                             uint64_t                              nr_of_leaves,
		                             uint64_t                             &level_to_write,
		                             Tresor::Type_1_node                  &child,
		                             Ft_initializer_channel::Type_2_level &child_level,
		                             Ft_initializer_channel::Child_state  &child_state,
		                             uint64_t                              level_index,
		                             uint64_t                              child_index);

		void _execute_inner_t1_child(bool                                 &progress,
		                             uint64_t                              nr_of_leaves,
		                             uint64_t                             &level_to_write,
		                             Tresor::Type_1_node                  &child,
		                             Ft_initializer_channel::Type_1_level &child_level,
		                             Ft_initializer_channel::Child_state  &child_state,
		                             uint64_t                              level_index,
		                             uint64_t                              child_index);

		void _mark_req_failed(bool       &progress,
		                      char const *str);

		void _mark_req_successful(bool    &progress);

		void _execute(bool    &progress);

		void _execute_init(bool    &progress);

	public:

		void execute(bool &);
};


class Tresor::Ft_initializer : public Module
{
	private:

		using Request = Ft_initializer_request;
		using Channel = Ft_initializer_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		bool new_submit_request() override { return false; }

	public:

		Ft_initializer();

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _TRESOR__FT_INITIALIZER_H_ */
