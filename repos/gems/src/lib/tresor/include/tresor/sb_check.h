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

				using Module = Sb_check;

				struct Attr { bool &out_success; };

			private:

				enum State {
					INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, CHECK_VBD, CHECK_VBD_SUCCEEDED, CHECK_FT, CHECK_FT_SUCCEEDED,
					CHECK_MT, CHECK_MT_SUCCEEDED};

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
				Generated_request<Check, Vbd_check::Check, State> _check_vbd { *this, _state, INIT };
				Generated_request<Check, Ft_check::Check, State> _check_ft { *this, _state, INIT };
				Generated_request<Check, Block_io_read, State> _read_blk { *this, _state, INIT };

				NONCOPYABLE(Check);

				void _mark_succeeded(bool &);

			public:

				Check(Attr attr) : _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "check"); }

				void mark_failed(bool &, Error_string);

				bool execute(Vbd_check &vbd_check, Ft_check &ft_check, Block_io &block_io);

				bool complete() const { return _state == COMPLETE; }
		};

		Sb_check() { }

		/*
		 * ANMERKUNG
		 *
		 * Pro (indirekt) angesprochenem Modul kommt ein Argument bei
		 * MODULE::execute(..) und REQUEST::execute(..) sowie diversen
		 * Sub-Calls hinzu (Beispiele: VBD: 5 Module, Superblock Control:
		 * 7 Module). Dies könnte durch eine Übergabe am Modul-Konstruktor und
		 * entsprechenden Member weniger invasiv gemacht werden (insofern man
		 * Requests Zugriff auf diese Member gewährt.
		 */

		bool execute(Check &check, Vbd_check &vbd_check, Ft_check &ft_check, Block_io &block_io) { return check.execute(vbd_check, ft_check, block_io); };
};

#endif /* _TRESOR__SB_CHECK_H_ */
