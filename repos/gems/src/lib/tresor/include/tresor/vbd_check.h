/*
 * \brief  Module for checking all hashes of a VBD snapshot
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VBD_CHECK_H_
#define _TRESOR__VBD_CHECK_H_

/* base includes */
#include <base/output.h>

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>

namespace Tresor {

	class Vbd_check;
	class Vbd_check_request;
	class Vbd_check_channel;
}


class Tresor::Vbd_check_request : public Module_request
{
	friend class Vbd_check;
	friend class Vbd_check_channel;

	private:

		Tree_level_index _max_lvl { 0 };
		Tree_node_index _max_child_idx { 0 };
		Number_of_leaves _nr_of_leaves { 0 };
		Type_1_node _root { };
		addr_t _success_ptr { };

		bool &_success() { return *(bool*)_success_ptr; }

	public:

		Vbd_check_request() { }

		Vbd_check_request(Module_id, Module_channel_id, Tree_level_index, Tree_node_index, Number_of_leaves,
		                  Type_1_node, bool &);

		void print(Output &out) const override { Genode::print(out, "check root ", _root); }
};


class Tresor::Vbd_check_channel : public Module_channel
{
	private:

		friend class Vbd_check;

		using Request = Vbd_check_request;

		enum Child_state {
			READ_BLOCK = 0, CHECK_HASH = 1, DONE = 2 };

		struct Type_1_level
		{
			Child_state       children_state[NR_OF_T1_NODES_PER_BLK] { };
			Type_1_node_block children                                   { };

			Type_1_level()
			{
				for (Child_state &state : children_state)
					state = DONE;
			}
		};

		enum Primitive_tag { INVALID, BLOCK_IO };

		struct Generated_primitive
		{
			bool                   success { false };
			Primitive_tag          tag     { INVALID };
			Physical_block_address blk_nr  { 0 };
			bool                   dropped { false };

			bool valid() const { return tag != INVALID; }
		};

		Generated_primitive _gen_prim { };
		Tree_level_index _lvl_to_read { 0 };
		Child_state _root_state { DONE };
		Block _leaf_lvl { };
		Block _encoded_blk { };
		Type_1_level _t1_lvls[TREE_MAX_LEVEL] { };
		Request _request { };
		Number_of_leaves _num_remaining_leaves { 0 };
		bool _generated_req_success { false };

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override { ASSERT_NEVER_REACHED; }

		bool _request_complete() override { return false; }

		void _reset();
};


class Tresor::Vbd_check : public Module
{
	private:

		using Request = Vbd_check_request;
		using Channel = Vbd_check_channel;
		using Child_state = Vbd_check_channel::Child_state;
		using Type_1_level = Vbd_check_channel::Type_1_level;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _execute_check(Channel &channel,
		                    bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);

		void _execute_inner_t1_child(Channel           &chan,
		                             Type_1_node const &child,
		                             Type_1_level      &child_lvl,
		                             Child_state       &child_state,
		                             Tree_level_index   lvl,
		                             Tree_node_index    child_idx,
		                             bool              &progress);


		void _execute_leaf_child(Channel           &chan,
		                         Type_1_node const &child,
		                         Block       const &child_lvl,
		                         Child_state       &child_state,
		                         Tree_level_index   lvl,
		                         Tree_node_index    child_idx,
		                         bool              &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		bool new_submit_request() override { return false; }


	public:

		Vbd_check() { register_channels(_channels, NR_OF_CHANNELS, VBD_CHECK); }


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _TRESOR__VBD_CHECK_H_ */
