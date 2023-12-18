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

/* tresor includes */
#include <tresor/vbd_check.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

bool Vbd_check_request::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	bool &check_node = _check_node[lvl][node_idx];
	if (!check_node)
		return false;

	Type_1_node const &node = _t1_blks.items[lvl].nodes[node_idx];
	switch (_state) {
	case REQ_IN_PROGRESS:

		if (lvl == 1) {
			if (!_num_remaining_leaves) {
				if (node.valid()) {
					_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node,
					                             ") valid but no leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, _vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": expectedly invalid");
				break;
			}
			if (node.gen == INITIAL_GENERATION) {
				_num_remaining_leaves--;
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, _vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": uninitialized");
				break;
			}
		} else {
			if (!node.valid()) {
				if (_num_remaining_leaves) {
					_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " invalid but ",
					                             _num_remaining_leaves, " leaves remaining" });
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, _vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": expectedly invalid");
				break;
			}
		}
		_generate_req(_read_blk, READ_BLK_SUCCEEDED, progress, node.pba, _blk);
		if (VERBOSE_CHECK)
			log(Level_indent { lvl, _vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node,
			    "): load to lvl ", lvl - 1);
		break;

	case READ_BLK_SUCCEEDED:

		if (!(lvl > 1 && node.gen == INITIAL_GENERATION) && !check_hash(_blk, node.hash)) {
			_mark_req_failed(progress, { "lvl ", lvl, " node ", node_idx, " (", node, ") has bad hash" });
			break;
		}
		if (lvl == 1)
			_num_remaining_leaves--;
		else {
			_t1_blks.items[lvl - 1].decode_from_blk(_blk);
			for (bool &cn : _check_node[lvl - 1])
				cn = true;
		}
		check_node = false;
		_state = REQ_IN_PROGRESS;
		progress = true;
		if (VERBOSE_CHECK)
			log(Level_indent { lvl, _vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": good hash");
		break;

	default: break;
	}
	return true;
}


bool Vbd_check_request::execute(Block_io &blk_io)
{
	bool progress;
	_execute_generated_req(blk_io, progress);

	if (_state == REQ_SUBMITTED) {
		for (Tree_level_index lvl { 1 }; lvl <= _vbd.max_lvl + 1; lvl++)
			for (Tree_node_index node_idx { 0 }; node_idx < _vbd.degree; node_idx++)
				_check_node[lvl][node_idx] = false;

		_num_remaining_leaves = _vbd.num_leaves;
		_t1_blks.items[_vbd.max_lvl + 1].nodes[0] = _vbd.t1_node();
		_check_node[_vbd.max_lvl + 1][0] = true;
		_state = REQ_IN_PROGRESS;
	}
	for (Tree_level_index lvl { 1 }; lvl <= _vbd.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _vbd.degree; node_idx++)
			if (_execute_node(lvl, node_idx, progress))
				return progress;

	_mark_req_successful(progress);
	return progress;
}


void Vbd_check_request::_mark_req_failed(bool &progress, Error_string str)
{
	error("vbd check request failed: ", str);
	_state = REQ_COMPLETE;
	progress = true;
}


void Vbd_check_request::_mark_req_successful(bool &progress)
{
	_state = REQ_COMPLETE;
	progress = true;
}
