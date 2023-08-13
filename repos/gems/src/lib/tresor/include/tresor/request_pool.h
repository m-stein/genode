/*
 * \brief  Module for scheduling requests for processing
 * \author Martin Stein
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__REQUEST_POOL_H_
#define _TRESOR__REQUEST_POOL_H_

/* tresor includes */
#include <tresor/module.h>
#include <tresor/types.h>
#include <tresor/vfs_utilities.h>
#include <tresor/superblock_control.h>

namespace Tresor {

	class Request;
	class Request_pool;
	class Request_pool_channel;
}

class Tresor::Request : public Module_request
{
	friend class Request_pool;
	friend class Request_pool_channel;

	public:

		enum Operation {
			INVALID, READ, WRITE, SYNC, CREATE_SNAPSHOT, DISCARD_SNAPSHOT,
			REKEY, EXTEND_VBD, EXTEND_FT, RESUME_REKEYING, DEINITIALIZE, INITIALIZE, };

	private:

		Operation _op { Operation::INVALID };
		bool _success { false };
		Virtual_block_address _vba { 0 };
		Request_offset _offset { 0 };
		Number_of_blocks _count { 0 };
		Key_id _key_id { 0 };
		Request_tag _tag { 0 };
		Generation _gen { 0 };

	public:

		Request(Operation op, bool success, Virtual_block_address vba, Request_offset offset,
		        Number_of_blocks count, Key_id key_id, Request_tag tag, Generation gen,
		        Module_id src_module_id, Module_request_id src_request_id)
		:
			Module_request { src_module_id, src_request_id, REQUEST_POOL },
			_op { op }, _success { success }, _vba { vba }, _offset { offset },
			_count { count }, _key_id { key_id }, _tag { tag }, _gen { gen }
		{ }

		Request(Operation op) : _op { op } { }

		Request() { }

		Operation op() const { return _op; }
		bool success() const { return _success; }
		void success(bool arg) { _success = arg; }
		Virtual_block_address vba() const { return _vba; }
		Request_offset offset() const { return _offset; }
		Number_of_blocks count() const { return _count; }
		Key_id key_id() const { return _key_id; }
		Request_tag tag() const { return _tag; }
		Generation gen() const { return _gen; }
		void gen(Generation arg) { _gen = arg; }

		static char const *op_to_string(Operation);


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override
		{
			Genode::print(out, op_to_string(_op));
			switch (_op) {
			case READ:
			case WRITE:
			case SYNC:
				if (_count > 1)
					Genode::print(out, " vbas ", _vba, "..", _vba + _count - 1);
				else
					Genode::print(out, " vba ", _vba);
				break;
			default: break;
			}
		}
};

class Tresor::Request_pool_channel : public Module_channel
{
	friend class Request_pool;

	private:

		enum State : State_uint {
			INVALID, REQ_SUBMITTED, REQ_RESUMED, REQ_GENERATED, REKEY_INIT_SUCCEEDED, PREPONED_REQUESTS_COMPLETE,
			TREE_EXTENSION_STEP_SUCCEEDED, FORWARD_TO_SB_CTRL_SUCCEEDED, ACCESS_VBA_AT_SB_CTRL_SUCCEEDED,
			REKEY_VBA_SUCCEEDED, INITIALIZE_SB_CTRL_SUCCEEDED, DEINITIALIZE_SB_CTRL_SUCCEEDED, REQ_COMPLETE };

		Tresor::Request _req { };
		State _state { INVALID };
		Number_of_blocks _num_blks { 0 };
		Virtual_block_address _vba { 0 };
		Superblock::State _sb_state { Superblock::INVALID };
		uint32_t _num_requests_preponed { 0 };
		bool _request_finished { false };
		bool _generated_req_success { false };

		void _generated_req_complete(State_uint) override;

		void _reset();
};

class Tresor::Request_pool : public Module
{
	private:

		using Channel = Request_pool_channel;
		using Channel_index = uint64_t;

		enum { MAX_NUM_REQUESTS_PREPONED = 8 };
		enum { NUM_CHANNELS = 16 };

		class Channel_index_queue
		{
			private:

				using Slot_index = uint32_t;

				Slot_index _head { 0 };
				Slot_index _tail { 0 };
				unsigned long _num_used_slots { 0 };
				Channel_index _slots[NUM_CHANNELS] { 0 };

			public:

				bool empty() const { return _num_used_slots == 0; }

				bool full() const { return _num_used_slots >= NUM_CHANNELS; }

				Channel_index head() const
				{
					ASSERT(!empty());
					return _slots[_head];
				}

				void enqueue(Channel_index const chan_idx)
				{
					ASSERT(!full());
					_slots[_tail] = chan_idx;
					_tail = (_tail + 1) % NUM_CHANNELS;
					_num_used_slots += 1;
				}

				void move_one_slot_towards_tail(Channel_index chan_idx)
				{
					Slot_index slot_idx { _head };
					Slot_index next_slot_idx;
					Channel_index chan_idx_buf;
					ASSERT(!empty());
					while (1) {
						if (slot_idx < NUM_CHANNELS - 1)
							next_slot_idx = slot_idx + 1;
						else
							next_slot_idx = 0;

						ASSERT(next_slot_idx != _tail);
						if (_slots[slot_idx] == chan_idx) {
							chan_idx_buf = _slots[next_slot_idx];
							_slots[next_slot_idx] = _slots[slot_idx];
							_slots[slot_idx] = chan_idx_buf;
							return;
						} else
							slot_idx = next_slot_idx;
					}
				}

				bool is_tail(Channel_index chan_idx) const
				{
					Slot_index slot_idx;
					ASSERT(!empty());
					if (_tail > 0)
						slot_idx = _tail - 1;
					else
						slot_idx = NUM_CHANNELS - 1;

					return _slots[slot_idx] == chan_idx;
				}

				Channel_index next(Channel_index chan_idx) const
				{
					Slot_index slot_idx { _head };
					Slot_index next_slot_idx;
					ASSERT(!empty());
					while (1) {
						if (slot_idx < NUM_CHANNELS - 1)
							next_slot_idx = slot_idx + 1;
						else
							next_slot_idx = 0;

						ASSERT(next_slot_idx != _tail);
						if (_slots[slot_idx] == chan_idx)
							return _slots[next_slot_idx];
						else
							slot_idx = next_slot_idx;
					}
				}

				void dequeue(Channel_index const chan_idx)
				{
					ASSERT(!empty() && head() == chan_idx);
					_head = (_head + 1) % NUM_CHANNELS;
					_num_used_slots -= 1;
				}
		};

		Channel _channels[NUM_CHANNELS] { };
		Channel_index_queue _chan_idx_queue { };

		static char const *_state_to_step_label(Channel::State);

		void _mark_req_successful(Channel &, Channel_index, bool &);

		bool _handle_failed_generated_req(Channel &, Channel_index, bool &, unsigned long);

		void _mark_req_failed(Channel &, bool &, unsigned long line);

		void _execute_rekey(Channel &, Channel_index, bool &);

		void _execute_extend_tree(Channel &, Channel_index, Superblock_control_request::Type, bool &);

		void _execute_initialize(Channel &, Channel_index, bool &);

		void _gen_superblock_control_req(Channel &, Channel_index, bool &, Superblock_control_request::Type, Virtual_block_address, Channel::State);

		void _forward_to_sb_ctrl(Channel &, Channel_index, bool &, Superblock_control_request::Type);

		void _execute_access_vbas(Channel &, Channel_index, bool &, Superblock_control_request::Type);

		void _try_prepone_requests(Channel &, Channel_index, bool &);

		void _resume_request(Channel &, Channel_index, bool &, Request::Operation);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(uint8_t *, size_t) override;

		void _drop_completed_request(Module_request &) override;

		void execute(bool &) override;

	public:

		Request_pool();


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override { return !_chan_idx_queue.full(); }

		void submit_request(Module_request &) override;
};

#endif /* _TRESOR__REQUEST_POOL_H_ */
