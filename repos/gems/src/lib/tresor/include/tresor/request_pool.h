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

/* base includes */
#include <util/list.h>

/* tresor includes */
#include <tresor/types.h>
#include <tresor/superblock_control.h>

namespace Tresor {

	class Request;
	class Request_pool;
	class Request_scheduler;
	class Request_pool_channel;
	class Request_pool_channel_queue;
}

class Tresor::Request : private List<Tresor::Request>::Element
{
	friend class Request_scheduler;
	friend class List<Tresor::Request>;

	public:

		using Module = Request_scheduler;

		enum Operation {
			READ, WRITE, SYNC, CREATE_SNAPSHOT, DISCARD_SNAPSHOT, REKEY, EXTEND_VBD,
			EXTEND_FT, RESUME_REKEYING, DEINITIALIZE, INITIALIZE, };

		struct Execute_attr
		{
			Superblock_control &sb_control;
			Client_data_interface &client_data;
			Virtual_block_device &vbd;
			Free_tree &free_tree;
			Meta_tree &meta_tree;
			Block_io &block_io;
			Trust_anchor &trust_anchor;
			Crypto &crypto;
		};

	private:

		Operation _op;
		Virtual_block_address const _vba;
		Request_offset const _offset;
		Number_of_blocks const _count;
		Key_id const _key_id;
		Request_tag const _tag;
		Generation &_gen;
		bool &_success;

		enum State { INIT, INIT_SB_CONTROL, INIT_SB_CONTROL_SUCCEEDED, COMPLETE };

		using Helper = Request_helper<Tresor::Request, State>;

		Helper _helper;
		Superblock::State _sb_state { Superblock::INVALID };
		union {
			Generatable_request<Helper, State, Superblock_control::Read_vbas> _read_vbas;
			Generatable_request<Helper, State, Superblock_control::Write_vbas> _write_vbas;
			Generatable_request<Helper, State, Superblock_control::Discard_snapshot> _discard_snap;
			Generatable_request<Helper, State, Superblock_control::Create_snapshot> _create_snap;
			Generatable_request<Helper, State, Superblock_control::Initialize> _init_sb_control;
			Generatable_request<Helper, State, Superblock_control::Deinitialize> _deinit_sb_control;
			Generatable_request<Helper, State, Superblock_control::Synchronize> _sync_sb_control;
			Generatable_request<Helper, State, Superblock_control::Rekey> _rekey;
			Generatable_request<Helper, State, Superblock_control::Extend_vbd> _extend_vbd;
			Generatable_request<Helper, State, Superblock_control::Extend_free_tree> _extend_ft;
		};

		bool execute(Execute_attr const &attr)
		{
log("Request::",__func__, __LINE__);
			bool progress = false;
			switch (_op) {
			case Request::INITIALIZE:

				switch (_helper.state) {
				case INIT: _init_sb_control.generate(_helper, INIT_SB_CONTROL, INIT_SB_CONTROL_SUCCEEDED, progress, _sb_state); break;
				case INIT_SB_CONTROL: progress |= _init_sb_control.execute(attr.sb_control, attr.block_io, attr.crypto, attr.trust_anchor); break;
				case INIT_SB_CONTROL_SUCCEEDED:

					switch (_sb_state) {
					case Superblock::NORMAL: _helper.mark_succeeded(progress); break;
					default: ASSERT_NEVER_REACHED;
					}
					break;

				default: break;
				}
				break;

			default: ASSERT_NEVER_REACHED;
			}
			return progress;
		}

	public:

		static char const *op_to_string(Operation);

		Request(Operation, Virtual_block_address, Request_offset,
		        Number_of_blocks, Key_id, Request_tag, Generation &, bool &);

		~Request() { }

		void print(Output &) const;

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};


class Tresor::Request_pool_channel_queue
{
	NONCOPYABLE(Request_pool_channel_queue);

	public:

		enum { NUM_SLOTS = 16 };

	private:

		using Channel = Request_pool_channel;
		using Slot_index = uint64_t;
		using Number_of_slots = uint64_t;

		Slot_index _head { 0 };
		Slot_index _tail { 0 };
		Number_of_slots _num_used_slots { 0 };
		Channel *_slots[NUM_SLOTS] { 0 };

	public:

		Request_pool_channel_queue() { }

		bool empty() const { return _num_used_slots == 0; }

		bool full() const { return _num_used_slots >= NUM_SLOTS; }

		Channel &head() const;

		void enqueue(Channel &);

		void move_one_slot_towards_tail(Channel const &);

		bool is_tail(Channel const &) const;

		Channel &next(Channel const &) const;

		void dequeue(Channel const &);
};


class Tresor::Request_scheduler : Noncopyable
{
	private:

		class Schedule : Noncopyable
		{
			private:

				Request *_tail { };
				List<Tresor::Request> _list { };

			public:

				void add_tail(Request &request)
				{
					_list.insert(&request, _tail);
					_tail = &request;
				}

				template <typename FN>
				void with_head(FN && fn)
				{
					if (_list.first())
						fn(*_list.first());
				}

				void remove_head()
				{
					Request *head = _list.first();
					if (!head)
						return;

					_list.remove(head);
					if (_tail == head)
						_tail = _list.first();
				}

				template <typename MOVE_BEHIND_FN>
				void move_head_backwards(MOVE_BEHIND_FN && can_move_behind)
				{
					Request *head = _list.first();
					if (!head)
						return;

					Request *next = head->List<Request>::Element::_next;
					Request *insert_head_at { };
					while (1) {
						if (!next)
							break;

						if (!can_move_behind(next))
							break;

						insert_head_at = next;
						next = next->List<Request>::Element::_next;
					}
					remove_head();
					_list.insert(head, insert_head_at);
				}
		};

		Schedule _schedule { };

	public:

		void add_request(Request &req)
		{
			_schedule.add_tail(req);
		}

		bool execute(Request::Execute_attr const &attr)
		{
log("Request_scheduler::",__func__, __LINE__);
			bool progress = false;
			_schedule.with_head([&] (Request &head) {
				progress |= head.execute(attr);
			});
			return progress;
		}

		static constexpr char const *name() { return "request_scheduler"; }
};

#endif /* _TRESOR__REQUEST_POOL_H_ */
