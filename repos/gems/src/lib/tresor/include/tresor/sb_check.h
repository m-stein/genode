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

/* tresor includes */
#include <tresor/types.h>
#include <tresor/vbd_check.h>

namespace Tresor {

	class Sb_check;
	class Sb_check_request;
	class Sb_check_channel;
}


class Tresor::Sb_check_request : public Module_request
{
	friend class Sb_check_channel;

	private:

		bool &_success;

		NONCOPYABLE(Sb_check_request);

	public:

		Sb_check_request(Module_id, Module_channel_id, bool &);

		void print(Output &out) const override { Genode::print(out, "check"); }
};


class Tresor::Sb_check_channel : public Module_channel
{
	private:

		using Request = Sb_check_request;

		enum State { REQ_SUBMITTED, REQ_COMPLETE, READ_BLK_SUCCESSFUL, REQ_GENERATED, REQ_GENERATED_NEW, CHECK_VBD_SUCCESSFUL, CHECK_FT_SUCCESSFUL, CHECK_MT_SUCCESSFUL};

		State _state { REQ_COMPLETE };
		Request *_req_ptr { };
		Generation _highest_gen { 0 };
		Superblock_index _highest_gen_sb_idx { 0 };
		bool _scan_for_highest_gen_sb_done { false };
		Superblock_index _sb_idx { 0 };
		Superblock _sb { };
		Snapshot_index _snap_idx { 0 };
		Constructible<Tree_root> _tree_root { };
		Block _blk { };
		Constructible<Vbd_check_request> _check_vbd { };
		State _generated_req_succeeded { REQ_COMPLETE };
		bool _generated_req_success { false };

		NONCOPYABLE(Sb_check_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		template <typename REQUEST, typename... ARGS>
		void _generate_req_new(Constructible<REQUEST> &req, State req_succeeded, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED_NEW;
			req.construct(typename REQUEST::Attr { args..., _generated_req_success });
			_generated_req_succeeded = req_succeeded;
			progress = true;
		}

		void _execute_generated_req(Vbd_check &vbd_chk, Block_io &blk_io, bool &progress)
		{
			if (_state != REQ_GENERATED_NEW)
				return;

			bool complete { false };
			if (_check_vbd.constructed()) {
				progress |= vbd_chk.execute_check(*_check_vbd, blk_io);
				complete = _check_vbd->complete();
			}
			if (complete) {
				if (!_generated_req_success) {
					_mark_req_failed(progress, "generated request");
					return;
				}
				_state = _generated_req_succeeded;
				progress = true;
			}
		}

		void _mark_req_failed(bool &, char const *);

		void _mark_req_successful(bool &);

	public:

		Sb_check_channel(Module_channel_id id) : Module_channel(SB_CHECK, id) { }

		void execute(Vbd_check &vbd_chk, Block_io &blk_io, bool &);
};


class Tresor::Sb_check : public Module
{
	private:

		using Channel = Sb_check_channel;

		Constructible<Channel> _channels[1] { };
		Vbd_check &_vbd_chk;
		Block_io &_blk_io;

		NONCOPYABLE(Sb_check);

	public:

		Sb_check(Vbd_check &vbd_check, Block_io &blk_io);

		void execute(bool &) override;
};

#endif /* _TRESOR__SB_CHECK_H_ */
