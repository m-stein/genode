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

/* tresor includes */
#include <tresor/sb_check.h>

using namespace Tresor;

bool Sb_check::Check::execute(Vbd_check &vbd_chk, Ft_check &ft_chk, Block_io &blk_io)
{
	bool progress = false;
	switch (_state) {
	case INIT:

		_highest_gen = 0;
		_highest_gen_sb_idx = 0;
		_snap_idx = 0;
		_sb_idx = 0;
		_scan_for_highest_gen_sb_done = false;
		_read_blk.generate(READ_BLK, READ_BLK_SUCCEEDED, progress, _sb_idx, _blk);
		break;

	case READ_BLK: progress |= _read_blk.execute(blk_io); break;
	case READ_BLK_SUCCEEDED:

		_sb.decode_from_blk(_blk);
		if (_scan_for_highest_gen_sb_done) {
			if (!_sb.valid()) {
				mark_failed(progress, "no valid superblock");;
				break;
			}
			Snapshot &snap { _sb.snapshots.items[_snap_idx] };
			if (snap.valid) {
				Snapshot &snap { _sb.snapshots.items[_snap_idx] };
				_tree_root.construct(snap.pba, snap.gen, snap.hash, snap.max_level, _sb.degree, snap.nr_of_leaves);
				_chk_vbd.generate(CHK_VBD, CHK_VBD_SUCCEEDED, progress, *_tree_root);
				if (VERBOSE_CHECK)
					log("  check snap ", _snap_idx, " (", snap, ")");
			} else {
				_state = CHK_VBD_SUCCEEDED;
				progress = true;
				if (VERBOSE_CHECK)
					log("  skip snap ", _snap_idx, " as it is unused");
			}
		} else {
			Snapshot &snap { _sb.curr_snap() };
			if (_sb.valid() && snap.gen > _highest_gen) {
				_highest_gen = snap.gen;
				_highest_gen_sb_idx = _sb_idx;
			}
			if (_sb_idx < MAX_SUPERBLOCK_INDEX) {
				_sb_idx++;
				_read_blk.generate(READ_BLK, READ_BLK_SUCCEEDED, progress, _sb_idx, _blk);
				progress = true;
			} else {
				_scan_for_highest_gen_sb_done = true;
				_read_blk.generate(READ_BLK, READ_BLK_SUCCEEDED, progress, _highest_gen_sb_idx, _blk);
				if (VERBOSE_CHECK)
					log("check superblock ", _highest_gen_sb_idx, "\n  read superblock");
			}
		}
		break;

	case CHK_VBD: progress |= _chk_vbd.execute(vbd_chk, blk_io); break;
	case CHK_VBD_SUCCEEDED:

		if (_snap_idx < MAX_SNAP_IDX) {
			_snap_idx++;
			_state = READ_BLK_SUCCEEDED;
			progress = true;
		} else {
			_snap_idx = 0;
			_tree_root.construct(_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree, _sb.free_leaves);
			_chk_ft.generate(CHK_FT, CHK_FT_SUCCEEDED, progress, *_tree_root);
			if (VERBOSE_CHECK)
				log("  check free tree");
		}
		break;

	case CHK_FT: progress |= _chk_ft.execute(ft_chk, blk_io); break;
	case CHK_FT_SUCCEEDED:

		_tree_root.construct(_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves);
		_chk_ft.generate(CHK_MT, CHK_MT_SUCCEEDED, progress, *_tree_root);
		if (VERBOSE_CHECK)
			log("  check meta tree");
		break;

	case CHK_MT: progress |= _chk_ft.execute(ft_chk, blk_io); break;
	case CHK_MT_SUCCEEDED: _mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


void Sb_check::Check::mark_failed(bool &progress, Error_string str)
{
	error("sb check: request (", *this, ") failed: \"", str, "\"");
	_attr.out_success = false;
	_state = COMPLETE;
	progress = true;
}


void Sb_check::Check::_mark_succeeded(bool &progress)
{
	_attr.out_success = true;
	_state = COMPLETE;
	progress = true;
}
