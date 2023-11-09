/*
 * \brief  Module for checking all hashes of a superblock and its hash trees
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__SB_CHECK_H_
#define _TRESOR__SB_CHECK_H_

/* base includes */
#include <base/output.h>

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>

namespace Tresor {

	class Sb_check;
	class Sb_check_request;
	class Sb_check_channel;
}


class Tresor::Sb_check_request : public Module_request
{
	private:

		friend class Sb_check;
		friend class Sb_check_channel;

		addr_t _success_ptr { };

		bool &_success() { return *(bool*)_success_ptr; }

	public:

		Sb_check_request() { }

		Sb_check_request(Module_id         src_module_id,
		                        Module_request_id src_request_id);

		Sb_check_request(Module_id, Module_request_id, bool &);

		void print(Output &out) const override { Genode::print(out, "check"); }
};


class Tresor::Sb_check_channel : public Module_channel
{
	private:

		friend class Sb_check;

		using Request = Sb_check_request;

		enum State { INSPECT_SBS, CHECK_SB };

		enum Sb_slot_state : State_uint {
			INACTIVE, INIT, DONE, READ_DONE, REQ_GENERATED,
			VBD_CHECK_STARTED, VBD_CHECK_DROPPED, VBD_CHECK_DONE,
			FT_CHECK_STARTED, FT_CHECK_DROPPED, FT_CHECK_DONE,
			MT_CHECK_STARTED, MT_CHECK_DROPPED, MT_CHECK_DONE };

		State _state { INSPECT_SBS };
		Request _request { };
		Generation _highest_gen { 0 };
		Superblock_index _last_sb_slot_idx { 0 };
		Sb_slot_state _sb_slot_state { INACTIVE };
		Superblock_index _sb_slot_idx { 0 };
		Superblock _sb_slot { };
		Snapshot_index _snap_idx { 0 };
		Type_1_node _vbd { };
		Type_1_node _ft { };
		Type_1_node _mt { };
		Physical_block_address _gen_prim_blk_nr { 0 };
		bool _gen_prim_success { false };
		Block _encoded_blk { };

		void _reset();

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override { ASSERT_NEVER_REACHED; }

		bool _request_complete() override { return false; }

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_sb_slot_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _gen_prim_success);
		}
};


class Tresor::Sb_check : public Module
{
	private:

		using Request = Sb_check_request;
		using Channel = Sb_check_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		static char const *_state_to_step_label(Channel::Sb_slot_state state);

		bool _handle_failed_generated_req(Channel &chan,
		                                  bool    &progress);

		void _execute_check(Channel &channel,
		                    bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		bool new_submit_request() override { return false; }


	public:

		Sb_check() { register_channels(_channels, NR_OF_CHANNELS, SB_CHECK); }

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _TRESOR__SB_CHECK_H_ */
