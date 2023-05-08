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

#ifndef _VBD_CHECK_H_
#define _VBD_CHECK_H_

/* Genode includes */
#include <base/output.h>

/* gems includes */
#include <cbe/types.h>

/* local includes */
#include <module.h>

namespace Cbe
{
	class Vbd_check;
	class Vbd_check_request;
	class Vbd_check_channel;
}


class Cbe::Vbd_check_request : public Module_request
{
	public:

		enum Type { INVALID = 0, CHECK = 1, };

	private:

		friend class Vbd_check;
		friend class Vbd_check_channel;

		Type                    _type          { INVALID };
		Tree_level_index        _max_lvl       { 0 };
		Type_1_node_block_index _max_child_idx { 0 };
		Tree_number_of_leaves   _nr_of_leaves  { 0 };
		Type_1_node             _root          { };
		bool                    _success       { false };

	public:

		Vbd_check_request() { }

		Vbd_check_request(Genode::uint64_t        src_module_id,
		                  Genode::uint64_t        src_request_id,
		                  Type                    type,
		                  Tree_level_index        max_lvl,
		                  Type_1_node_block_index max_child_idx,
		                  Tree_number_of_leaves   nr_of_leaves,
		                  Type_1_node             root);

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Genode::Output &out) const override
		{
			Genode::print(out, type_to_string(_type), " root ", _root);
		}
};


class Cbe::Vbd_check_channel
{
	private:

		friend class Vbd_check;

		using Request = Vbd_check_request;

		enum Child_state {
			READ_BLOCK = 0, CHECK_HASH = 1, DONE = 2 };

		struct Type_1_level
		{
			Child_state       children_state[NR_OF_TYPE_1_NODES_PER_BLK] { };
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

		Generated_primitive _gen_prim                { };
		Tree_level_index    _lvl_to_read             { 0 };
		Child_state         _root_state              { DONE };
		Block_data          _leaf_lvl                { };
		Type_1_level        _t1_lvls[TREE_MAX_LEVEL] { };
		Request             _request                 { };
};


class Cbe::Vbd_check : public Module
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

		void _execute_inner_t1_child(Channel                 &chan,
		                             Type_1_node       const &child,
		                             Type_1_level            &child_lvl,
		                             Child_state             &child_state,
		                             Tree_level_index         lvl,
		                             Type_1_node_block_index  child_idx,
		                             bool                    &progress);


		void _execute_leaf_child(Channel                 &chan,
		                         Type_1_node       const &child,
		                         Block_data        const &child_lvl,
		                         Child_state             &child_state,
		                         Tree_level_index         lvl,
		                         Type_1_node_block_index  child_idx,
		                         bool                    &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;


	public:

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _VBD_CHECK_H_ */
