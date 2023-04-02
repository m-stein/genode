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

#ifndef _VBD_INITIALIZER_H_
#define _VBD_INITIALIZER_H_

/* Genode includes */
#include <base/output.h>

/* cbe tester includes */
#include "module.h"

namespace Cbe
{
	class Vbd_initializer;
	class Vbd_initializer_request;
	class Vbd_initializer_channel;
}


class Cbe::Vbd_initializer_request : public Module_request
{
	public:

		enum Type { INVALID = 0, INIT = 1, };

	private:

		friend class Vbd_initializer;
		friend class Vbd_initializer_channel;

		Type             _type                                { INVALID };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE]                 { 0 };
		Genode::uint8_t  _root_node[sizeof(Cbe::Type_1_node)] { 0 };
		Genode::uint64_t _max_level_idx                       { 0 };
		Genode::uint64_t _max_child_idx                       { 0 };
		Genode::uint64_t _nr_of_leaves                        { 0 };
		bool             _success                             { false };


	public:

		Vbd_initializer_request() { }

		Vbd_initializer_request(unsigned long src_module_id,
		                        unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   size_t            buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   Genode::size_t    prim_size,
		                   Genode::uint64_t  max_level_idx,
		                   Genode::uint64_t  max_child_idx,
		                   Genode::uint64_t  nr_of_leaves);

		void *prim_ptr() { return (void *)&_prim; }

		void *root_node() { return _root_node; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Genode::Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Cbe::Vbd_initializer_channel
{
	private:

		friend class Vbd_initializer;

		enum State {
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE,
			BLOCK_ALLOC_PENDING,
			BLOCK_ALLOC_IN_PROGRESS,
			BLOCK_ALLOC_COMPLETE,
			BLOCK_IO_PENDING,
			BLOCK_IO_IN_PROGRESS,
			BLOCK_IO_COMPLETE,
		};

		State                   _state   { INACTIVE };
		Vbd_initializer_request _request { };

		enum Child_state { DONE, INIT_BLOCK, INIT_NODE, WRITE_BLOCK, };

		struct Type_1_level
		{
			Type_1_node_block children { };
			Child_state       children_state[NR_OF_TYPE_1_NODES_PER_BLK] { DONE };
		};

		struct Root_node
		{
			Type_1_node node  { };
			Child_state state { DONE };
		};

		Root_node    _root_node                 { };
		Type_1_level _t1_levels[TREE_MAX_LEVEL] { };

		Genode::uint64_t _level_to_write { 0 };

		static void reset_node(Cbe::Type_1_node &node)
		{
			memset(&node, 0, sizeof(Type_1_node));
		}

		static void reset_level(Type_1_level &level,
		                        Child_state   state)
		{
			for (unsigned int i = 0; i < NR_OF_TYPE_1_NODES_PER_BLK; i++) {
				reset_node(level.children.nodes[i]);
				level.children_state[i] = state;
			}
		}

		static void dump(Type_1_node_block const &node_block)
		{
			for (auto v : node_block.nodes) {
				Genode::log(v);
			}
		}

		/* Block_allocator */
		Genode::uint64_t _blk_nr { 0 };

		/* Block_io */
		Genode::uint64_t _child_pba { 0 };

		bool _generated_req_success { false };
};


class Cbe::Vbd_initializer : public Module
{
	private:

		using Request = Vbd_initializer_request;
		using Channel = Vbd_initializer_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _execute_leaf_child(Channel                              &channel,
		                         bool                                 &progress,
                                 Genode::uint64_t                     &nr_of_leaves,
		                         Cbe::Type_1_node                     &child,
		                         Vbd_initializer_channel::Child_state &child_state,
		                         Genode::uint64_t                      level_index,
		                         Genode::uint64_t                      child_index);

		void _execute_inner_t1_child(Channel                               &channel,
		                             bool                                  &progress,
		                             Genode::uint64_t                       nr_of_leaves,
		                             Genode::uint64_t                      &level_to_write,
		                             Cbe::Type_1_node                      &child,
		                             Vbd_initializer_channel::Type_1_level &child_level,
		                             Vbd_initializer_channel::Child_state  &child_state,
		                             Genode::uint64_t                       level_index,
		                             Genode::uint64_t                       child_index);

		void _execute(Channel &channel,
		              bool    &progress);

		void _execute_init(Channel &channel,
		                   bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


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

		Vbd_initializer();

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _VBD_INITIALIZER_H_ */
