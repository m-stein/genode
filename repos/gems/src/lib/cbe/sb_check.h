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

#ifndef _SB_CHECK_H_
#define _SB_CHECK_H_

/* Genode includes */
#include <base/output.h>

/* gems includes */
#include <cbe/types.h>

/* local includes */
#include <module.h>

namespace Cbe
{
	class Sb_check;
	class Sb_check_request;
	class Sb_check_channel;
}


class Cbe::Sb_check_request : public Module_request
{
	public:

		enum Type { INVALID = 0, CHECK = 1, };

	private:

		friend class Sb_check;
		friend class Sb_check_channel;

		Type _type    { INVALID };
		bool _success { false };


	public:

		Sb_check_request() { }

		Sb_check_request(unsigned long src_module_id,
		                        unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type);

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Genode::Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Cbe::Sb_check_channel
{
	private:

		friend class Sb_check;

		using Request = Sb_check_request;

		enum State { INSPECT_SBS, CHECK_SB };

		enum Sb_slot_state {
			INACTIVE, INIT, DONE,
			READ_STARTED, READ_DROPPED, READ_DONE,
			VBD_CHECK_STARTED, VBD_CHECK_DROPPED, VBD_CHECK_DONE,
			FT_CHECK_STARTED, FT_CHECK_DROPPED, FT_CHECK_DONE,
			MT_CHECK_STARTED, MT_CHECK_DROPPED, MT_CHECK_DONE };

		State                  _state            { INSPECT_SBS };
		Request                _request          { };
		Generation             _highest_gen      { 0 };
		Superblocks_index      _last_sb_slot_idx { 0 };
		Sb_slot_state          _sb_slot_state    { INACTIVE };
		Superblocks_index      _sb_slot_idx      { 0 };
		Superblock             _sb_slot          { };
		Snapshots_index        _snap_idx         { 0 };
		Type_1_node            _vbd              { };
		Type_1_node            _ft               { };
		Type_1_node            _mt               { };
		Physical_block_address _gen_prim_blk_nr  { 0 };
		bool                   _gen_prim_success { false };
};


class Cbe::Sb_check : public Module
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

#endif /* _SB_CHECK_H_ */
