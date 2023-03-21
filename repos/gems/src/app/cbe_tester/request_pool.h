/*
 * \brief  Module for request pool
 * \author Martin Stein
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REQUEST_POOL_H_
#define _REQUEST_POOL_H_

/* cbe tester includes */
#include <module.h>
#include <cbe/types.h>
#include <vfs_utilities.h>

namespace Cbe
{
	class Request_pool;
	class Request_pool_request;
	class Request_pool_channel;
}

class Cbe::Request_pool_channel
{
	private:

		friend class Request_pool;

		enum State {
			INVALID,
			SUBMITTED,
			SUBMITTED_RESUME_REKEYING,
			REKEY_INIT_PENDING,
			REKEY_INIT_IN_PROGRESS,
			REKEY_INIT_COMPLETE,
			PREPONE_REQUESTS_PENDING,
			PREPONE_REQUESTS_COMPLETE,
			VBD_EXTENSION_STEP_PENDING,
			VBD_EXTENSION_STEP_IN_PROGRESS,
			VBD_EXTENSION_STEP_COMPLETE,
			FT_EXTENSION_STEP_PENDING,
			FT_EXTENSION_STEP_IN_PROGRESS,
			FT_EXTENSION_STEP_COMPLETE,
			CREATE_SNAP_AT_SB_CTRL_PENDING,
			CREATE_SNAP_AT_SB_CTRL_IN_PROGRESS,
			CREATE_SNAP_AT_SB_CTRL_COMPLETE,
			SYNC_AT_SB_CTRL_PENDING,
			SYNC_AT_SB_CTRL_IN_PROGRESS,
			SYNC_AT_SB_CTRL_COMPLETE,
			READ_VBA_AT_SB_CTRL_PENDING,
			READ_VBA_AT_SB_CTRL_IN_PROGRESS,
			READ_VBA_AT_SB_CTRL_COMPLETE,
			WRITE_VBA_AT_SB_CTRL_PENDING,
			WRITE_VBA_AT_SB_CTRL_IN_PROGRESS,
			WRITE_VBA_AT_SB_CTRL_COMPLETE,
			DISCARD_SNAP_AT_SB_CTRL_PENDING,
			DISCARD_SNAP_AT_SB_CTRL_IN_PROGRESS,
			DISCARD_SNAP_AT_SB_CTRL_COMPLETE,
			REKEY_VBA_PENDING,
			REKEY_VBA_IN_PROGRESS,
			REKEY_VBA_COMPLETE,
			INITIALIZE_SB_CTRL_PENDING,
			INITIALIZE_SB_CTRL_IN_PROGRESS,
			INITIALIZE_SB_CTRL_COMPLETE,
			DEINITIALIZE_SB_CTRL_PENDING,
			DEINITIALIZE_SB_CTRL_IN_PROGRESS,
			DEINITIALIZE_SB_CTRL_COMPLETE,
			COMPLETE
		};

		enum Tag_type {
			TAG_POOL_SB_CTRL_READ_VBA,
			TAG_POOL_SB_CTRL_WRITE_VBA,
			TAG_POOL_SB_CTRL_SYNC,
			TAG_POOL_SB_CTRL_INITIALIZE,
			TAG_POOL_SB_CTRL_DEINITIALIZE,
		};

		using Index_type = Genode::uint32_t; /* XXX */

		struct Generated_prim {
			enum Type { READ, WRITE };

			Type       op;
			bool       succ;
			Tag_type   tg;
			Index_type pl_idx;
			uint64_t   blk_nr;
			uint64_t   idx;
		};

		Cbe::Request     _request    { };
		State            _state      { INVALID };
		Generated_prim   _prim       { };
		uint64_t         _nr_of_blks { 0 };
		Generation       _gen        { };
		Superblock_state _sb_state   { Superblock_state::INVALID };

		void invalidate()
		{
			_request    = { };
			_state      = { INVALID };
			_prim       = { };
			_nr_of_blks =  0;
			_gen        = { };
			_sb_state   = { Superblock_state::INVALID };
		}
};

class Cbe::Request_pool : public Module
{
	private:

		using Channel = Request_pool_channel;
		using Request = Cbe::Request;

		enum { MAX_NUMBER_OF_REQUESTS_IN_POOL = 16 };

		using Slots_index          = Genode::uint32_t; /* XXX */

		struct Index_queue {
			Slots_index          _head { };
			Slots_index          _tail { };
			unsigned             _nr_of_used_slots { };
			Slots_index          _slots[MAX_NUMBER_OF_REQUESTS_IN_POOL] { };

			bool empty() const { return _nr_of_used_slots == 0; }

			bool full() const {
				return _nr_of_used_slots >= MAX_NUMBER_OF_REQUESTS_IN_POOL; }

			Slots_index head() const
			{
				if (empty()) {
					class Index_queue_empty_head { };
					throw Index_queue_empty_head { };
				}
				return _slots[_head];
			}

			void enqueue(Slots_index const idx)
			{
				if (full()) {
					class Index_queue_enqueue_full { };
					throw Index_queue_enqueue_full { };
				}

				_slots[_tail] = idx;

				_tail = (_tail + 1) % MAX_NUMBER_OF_REQUESTS_IN_POOL;

				_nr_of_used_slots += 1;
			}

			void dequeue(Slots_index const idx)
			{
				if (empty() or _head != idx) {
					class Index_queue_dequeue_error { };
					throw Index_queue_dequeue_error { };
				}

				_head = (_head + 1) % MAX_NUMBER_OF_REQUESTS_IN_POOL;

				_nr_of_used_slots -= 1;
			}
		};

		enum { NR_OF_CHANNELS = 1 }; /* XXX */

		Channel _channels[NR_OF_CHANNELS] { };

		Index_queue      _indices { };

		void _execute_read (Channel &, Index_queue &, Slots_index const, bool &);
		void _execute_write(Channel &, Index_queue &, Slots_index const, bool &);
		void _execute_sync (Channel &, Index_queue &, Slots_index const, bool &);

		void _execute_initialize(Channel &, Index_queue &, Slots_index const,
		                         bool &);
		void _execute_deinitialize(Channel &, Index_queue &, Slots_index const,
		                           bool &);

		/************
		 ** Module **
		 ************/

		void execute(bool &) override;
};

#endif /* _REQUEST_POOL_H_ */
