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

/* base includes */
#include <base/output.h>

/* tresor includes */
#include <tresor/module.h>

namespace Tresor {

	class Vbd_initializer;
	class Vbd_initializer_request;
	class Vbd_initializer_channel;
}


class Tresor::Vbd_initializer_request : public Module_request
{
	public:

		enum Type { INVALID = 0, INIT = 1, };

	private:

		friend class Vbd_initializer;
		friend class Vbd_initializer_channel;

		Type     _type                           { INVALID };
		uint8_t  _root_node[sizeof(Type_1_node)] { 0 };
		uint64_t _max_level_idx                  { 0 };
		uint64_t _max_child_idx                  { 0 };
		uint64_t _nr_of_leaves                   { 0 };
		addr_t   _pba_alloc_ptr                  { 0 };
		bool     _success                        { false };

		Pba_allocator &_pba_alloc() { return *(Pba_allocator *)_pba_alloc_ptr; }

	public:

		Vbd_initializer_request() { }

		Vbd_initializer_request(Module_id         src_module_id,
		                        Module_request_id src_request_id);

		Vbd_initializer_request(Module_id         src_module_id,
		                        Module_request_id src_request_id,
		                   Type    req_type,
		                   Tree_level_index  max_level_idx,
		                   Tree_node_index  max_child_idx,
		                   Number_of_leaves  nr_of_leaves,
		                   Pba_allocator &pba_alloc);

		static void create(void     *buf_ptr,
		                   size_t    buf_size,
		                   uint64_t  src_module_id,
		                   uint64_t  src_request_id,
		                   size_t    req_type,
		                   uint64_t  max_level_idx,
		                   uint64_t  max_child_idx,
		                   uint64_t  nr_of_leaves,
		                   Pba_allocator &pba_alloc);

		void *root_node() { return _root_node; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Tresor::Vbd_initializer_channel : public Module_channel
{
	private:

		friend class Vbd_initializer;

		enum State { REQ_GENERATED, INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE, BLOCK_IO_COMPLETE };

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

		State _state { INACTIVE };
		Vbd_initializer_request _request { };
		Root_node _root_node { };
		Type_1_level _t1_levels[TREE_MAX_LEVEL] { };
		uint64_t _level_to_write { 0 };
		uint64_t _child_pba { 0 };
		bool _generated_req_success { false };
		Block _encoded_blk { };
		Hash _dummy_hash { };

		void _generated_req_completed(State_uint) override;

		bool _request_complete() override { return _state == COMPLETE; }

		void _request_submitted(Module_request &) override { ASSERT_NEVER_REACHED; }

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


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		bool new_submit_request() override { return false; }


	public:

		Vbd_initializer();

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _TRESOR__VBD_INITIALIZER_H_ */
