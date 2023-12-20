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
#include <tresor/ft_check.h>
#include <tresor/block_io.h>

namespace Tresor { class Sb_check; }

class Tresor::Sb_check
{
	private:

		NONCOPYABLE(Sb_check);

	public:

		class Check
		{
			public:

				struct Attr { bool &out_success; };

			private:

				enum State { INIT, COMPLETE, READ_BLK_SUCCEEDED, REQ_GENERATED, CHK_VBD_SUCCEEDED, CHK_FT_SUCCEEDED, CHK_MT_SUCCEEDED};

				Attr _attr;
				State _state { INIT };
				Generation _highest_gen { 0 };
				Superblock_index _highest_gen_sb_idx { 0 };
				bool _scan_for_highest_gen_sb_done { false };
				Superblock_index _sb_idx { 0 };
				Superblock _sb { };
				Snapshot_index _snap_idx { 0 };
				Constructible<Tree_root> _tree_root { };
				Block _blk { };
				State _generated_req_succeeded { INIT };
				bool _generated_req_success { false };
				Constructible<Vbd_check::Check> _check_vbd { };
				Constructible<Ft_check::Check> _check_ft { };
				Constructible<Block_io_read> _read_blk { };

				NONCOPYABLE(Check);

				template <typename REQUEST, typename... ARGS>
				void _generate_req(Constructible<REQUEST> &req, State req_succeeded, bool &progress, ARGS &&... args)
				{
					_state = REQ_GENERATED;
					req.construct(typename REQUEST::Attr { args..., _generated_req_success });
					_generated_req_succeeded = req_succeeded;
					progress = true;
				}

				void _execute_generated_req(Vbd_check &vbd_chk, Ft_check &ft_chk, Block_io &blk_io, bool &progress)
				{
					if (_state != REQ_GENERATED)
						return;

					bool complete { false };
					if (_check_vbd.constructed()) {
						progress |= vbd_chk.execute_check(*_check_vbd, blk_io);
						complete = _check_vbd->complete();
						if (complete)
							_check_vbd.destruct();
					}
					if (_check_ft.constructed()) {
						progress |= ft_chk.execute_check(*_check_ft, blk_io);
						complete = _check_ft->complete();
						if (complete)
							_check_ft.destruct();
					}
					if (_read_blk.constructed()) {
						progress |= blk_io.execute_read(*_read_blk);
						complete = _read_blk->complete();
						if (complete)
							_read_blk.destruct();
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

				Check(Attr attr) : _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "check"); }

				bool execute(Vbd_check &vbd_chk, Ft_check &ft_chk, Block_io &blk_io);

				bool complete() const { return _state == COMPLETE; }
		};

		Sb_check() { }

		bool execute_check(Check &chk, Vbd_check &vbd_chk, Ft_check &ft_chk, Block_io &blk_io) { return chk.execute(vbd_chk, ft_chk, blk_io); };
};

#endif /* _TRESOR__SB_CHECK_H_ */
