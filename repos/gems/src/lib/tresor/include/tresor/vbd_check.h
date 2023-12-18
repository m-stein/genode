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
	public:

		struct Attr
		{
			Tree_root const &in_vbd;
			bool &out_success;
		};

	private:

		enum State { INIT, IN_PROGRESS, COMPLETE, REQ_GENERATED, READ_BLK_SUCCEEDED };

		Attr _attr;
		State _state { INIT };
		Type_1_node_block_walk _t1_blks { };
		bool _check_node[TREE_MAX_NR_OF_LEVELS][NUM_NODES_PER_BLK] { };
		Block _blk { };
		Number_of_leaves _num_remaining_leaves { 0 };
		bool _generated_req_success { };
		State _generated_req_succeeded { COMPLETE };
		Constructible<Block_io_read> _read_blk { };

		NONCOPYABLE(Vbd_check_request);

		/*
		 * FIXME
		 *
		 * Methoden '_generate_req', 'complete' und '_execute_generated_req'
		 * bzw. der Aufruf von letzterem sind strukturell gleich für alle
		 * Module.
		 */

		template <typename REQUEST, typename... ARGS>
		void _generate_req(Constructible<REQUEST> &req, State req_succeeded, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			req.construct(typename REQUEST::Attr { args..., _generated_req_success });
			_generated_req_succeeded = req_succeeded;
			progress = true;
		}

		void _execute_generated_req(Block_io &blk_io, bool &progress)
		{
			if (_state != REQ_GENERATED)
				return;

			/*
			 * FIXME
			 *
			 * Wird eine recht lange, repetitive Liste bei einigen Modulen.
			 *
			 * Beispiele:
			 *   * VBD > 10 Request-Typen
			 *   * Superblock Control > 20 Request-Typen
			 */
			if (_read_blk.constructed()) {
				progress |= blk_io.execute_read(*_read_blk);
				if (_read_blk->complete()) {
					_read_blk.destruct();
					if (!_generated_req_success) {
						_mark_req_failed(progress, "generated request");
						return;
					}
					_state = _generated_req_succeeded;
					progress = true;
					return;
				}
			}
		}

		void _mark_req_failed(bool &, Error_string);

		void _mark_req_successful(bool &);

		bool _execute_node(Tree_level_index, Tree_node_index, bool &);

	public:

		Vbd_check_request(Attr attr) : _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "check ", _attr.in_vbd); }

		/*
		 * FIXME
		 *
		 * Pro (indirekt) angesprochenem Module kommt ein Argument bei
		 * MODULE::execute_REQUEST(..) und REQUEST::execute(..) hinzu.
		 *
		 * Beispiele:
		 *   * VBD: 5 Module
		 *   * Superblock Control: 7 Module
		 */
		bool execute(Block_io &);

		bool complete() const { return _state == COMPLETE; }
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
