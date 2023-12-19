/*
 * \brief  Module for checking all hashes of a free tree or meta tree
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FT_CHECK_H_
#define _TRESOR__FT_CHECK_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/block_io.h>

namespace Tresor { class Ft_check; }

class Tresor::Ft_check
{
	private:

		NONCOPYABLE(Ft_check);

	public:

		class Check
		{
			public:

				struct Attr
				{
					Tree_root const &in_ft;
					bool &out_success;
				};

			private:

				enum State { INIT, IN_PROGRESS, COMPLETE, REQ_GENERATED, READ_BLK_SUCCEEDED };

				Attr const _attr;
				State _state { INIT };
				Type_1_node_block_walk _t1_blks { };
				Type_2_node_block _t2_blk { };
				bool _check_node[TREE_MAX_NR_OF_LEVELS + 1][NUM_NODES_PER_BLK] { };
				Number_of_leaves _num_remaining_leaves { 0 };
				Block _blk { };
				bool _generated_req_success { false };
				State _generated_req_succeeded { INIT };
				Constructible<Block_io_read> _read_blk { };

				NONCOPYABLE(Check);

				void _mark_req_failed(bool &, Error_string);

				void _mark_req_successful(bool &);

				bool _execute_node(Tree_level_index, Tree_node_index, bool &);

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

					bool complete = false;
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

			public:

				Check(Attr attr) : _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "check ", _attr.in_ft); }

				bool execute(Block_io &);

				bool complete() const { return _state == COMPLETE; }
		};

		Ft_check() { }

		bool execute_check(Check &req, Block_io &blk_io) { return req.execute(blk_io); }
};

#endif /* _TRESOR__FT_CHECK_H_ */
