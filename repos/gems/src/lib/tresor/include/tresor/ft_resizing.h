/*
 * \brief  Module for file tree resizing
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FT_RESIZING_H_
#define _TRESOR__FT_RESIZING_H_

/* tresor includes */
#include <tresor/module.h>
#include <tresor/types.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Ft_resizing;
	class Ft_resizing_request;
	class Ft_resizing_channel;
}


class Tresor::Ft_resizing_request : public Module_request
{
	friend class Ft_resizing_channel;

	public:

		enum Type { EXTENSION_STEP };

	private:

		Type const _type;
		Generation const _curr_gen;
		Free_tree_root &_ft;
		Meta_tree_root &_mt;
		Physical_block_address &_pba;
		Number_of_blocks &_num_pbas;
		bool &_success;

		NONCOPYABLE(Ft_resizing_request);

	public:

		Ft_resizing_request(Module_id, Module_channel_id, Type, Generation, Free_tree_root &,
		                    Meta_tree_root &, Physical_block_address &, Number_of_blocks &, bool &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type), " root ", _ft); }
};

class Tresor::Ft_resizing_channel : public Module_channel
{
	private:

		using Request = Ft_resizing_request;

		enum State {
			REQ_SUBMITTED, READ_ROOT_NODE_COMPLETED, READ_INNER_NODE_COMPLETED,
			ALLOC_PBA_COMPLETED, WRITE_INNER_NODE_COMPLETED, WRITE_ROOT_NODE_COMPLETED,
			REQ_GENERATED, REQ_COMPLETE };

		Request *_req_ptr { nullptr };
		State _state { REQ_COMPLETE };
		Physical_block_address _alloc_pba { 0 };
		Type_1_node_block_walk _t1_blks { };
		Type_2_node_block _t2_blk { };
		Tree_level_index _lvl { 0 };
		Tree_level_index _alloc_lvl_idx { 0 };
		Virtual_block_address _vba { };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_generations _old_generations { };
		Tree_walk_pbas _new_pbas { };
		Block _encoded_blk { };
		Number_of_leaves _nr_of_leaves { 0 };
		Hash _dummy_hash { };
		bool _generated_req_success { };

		NONCOPYABLE(Ft_resizing_channel);

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _add_new_branch_to_ft_using_pba_contingent(Tree_level_index const,
		                                                Tree_node_index const,
		                                                Tree_degree const,
		                                                Generation const,
		                                                Physical_block_address &,
		                                                Number_of_blocks &,
		                                                Type_1_node_block_walk &,
		                                                Type_2_node_block &,
		                                                Tree_walk_pbas &,
		                                                Tree_level_index &,
		                                                Number_of_leaves &);

		void _add_new_root_lvl_to_ft_using_pba_contingent(Free_tree_root &,
		                                                  Generation const,
		                                                  Type_1_node_block_walk &,
		                                                  Tree_walk_pbas &,
		                                                  Physical_block_address &,
		                                                  Number_of_blocks &);

		void _set_args_for_write_back_of_inner_lvl(Tree_level_index const,
		                                           Tree_level_index const,
		                                           Physical_block_address const,
		                                           bool &progress);

		void _extension_step(bool &);

	public:

		void execute(bool &);

		Ft_resizing_channel(Module_channel_id id) : Module_channel { FT_RESIZING, id } { }
};

class Tresor::Ft_resizing : public Module
{
	private:

		using Channel = Ft_resizing_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Ft_resizing);

	public:

		void execute(bool &) override;

		Ft_resizing();
};

#endif /* _TRESOR__FT_RESIZING_H_ */
