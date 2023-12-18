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

#ifndef _TRESOR__VBD_CHECK_H_
#define _TRESOR__VBD_CHECK_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/block_io.h>

namespace Tresor {

	class Vbd_check;
	class Vbd_check_request;
}

class Tresor::Vbd_check_request
{
	private:

		enum State { REQ_SUBMITTED, REQ_IN_PROGRESS, REQ_COMPLETE, REQ_GENERATED, READ_BLK_SUCCEEDED };

		Tree_root const &_vbd;
		bool &_success;
		State _state { REQ_COMPLETE };
		Type_1_node_block_walk _t1_blks { };
		bool _check_node[TREE_MAX_NR_OF_LEVELS][NUM_NODES_PER_BLK] { };
		Block _blk { };
		Number_of_leaves _num_remaining_leaves { 0 };
		bool _generated_req_success { false };
		State _generated_req_complete { REQ_COMPLETE };
		Constructible<Block_io_read> _read_blk { };

		NONCOPYABLE(Vbd_check_request);

		template <typename REQUEST, typename... ARGS>
		void _generate_req(Constructible<REQUEST> &req, State req_complete, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			req.construct(args..., _generated_req_success);
			_generated_req_complete = req_complete;
			progress = true;
		}

		void _execute_generated_req(Block_io &blk_io, bool &progress)
		{
			if (_state != REQ_GENERATED)
				return;

			if (_read_blk.constructed()) {
				progress |= blk_io.execute_read(*_read_blk);
				if (_read_blk->complete())
					_state = _generated_req_complete;
			}
			/*
			 * FIXME
			 *
			 * Das wird eine recht lange repetitive Liste bei einigen Modulen
			 * aber ich weiß nicht wie ich es ohne Vererbung abmildern soll.
			 *
			 * Beispiele:
			 *   * VBD > 10 Request-Typen
			 *   * Superblock Control > 20 Request-Typen
			 */
		}

		void _mark_req_failed(bool &, Error_string);

		void _mark_req_successful(bool &);

		bool _execute_node(Tree_level_index, Tree_node_index, bool &);

	public:

		Vbd_check_request(Tree_root const &vbd, bool &success) : _vbd(vbd), _success(success) { }

		void print(Output &out) const { Genode::print(out, "check ", _vbd); }

		/*
		 * FIXME
		 *
		 * Pro (indirekt) angesprochenem Module ein Argument.
		 *
		 * Beispiele:
		 *   * VBD: 5 Module
		 *   * Superblock Control: 7 Module
		 */
		bool execute(Block_io &);

		bool complete() const { return _state == REQ_COMPLETE; }
};

class Tresor::Vbd_check
{
	private:

		NONCOPYABLE(Vbd_check);

	public:

		Vbd_check() { }

		bool execute_check(Vbd_check_request &req, Block_io &blk_io) { return req.execute(blk_io); }
};

#endif /* _TRESOR__VBD_CHECK_H_ */
