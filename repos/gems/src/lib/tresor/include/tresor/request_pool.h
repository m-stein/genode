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
	class Initializing_request_scheduler;
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

		enum State { INIT, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, COMPLETE };

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
			bool progress = false;
			switch (_op) {
			case Request::INITIALIZE:

				switch (_helper.state) {
				case INIT: _init_sb_control.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _sb_state); break;
				case SB_CONTROL_REQ: progress |= _init_sb_control.execute(attr.sb_control, attr.block_io, attr.crypto, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED: _helper.mark_succeeded(progress); break;
				default: break;
				}
				break;

			case Request::READ:

				switch (_helper.state) {
				case INIT: _read_vbas.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _vba, _count, _offset, _tag); break;
				case SB_CONTROL_REQ: progress |= _read_vbas.execute(attr.sb_control, attr.vbd, attr.client_data, attr.block_io, attr.crypto); break;
				case SB_CONTROL_REQ_SUCCEEDED: _helper.mark_succeeded(progress); break;
				default: break;
				}
				break;

			case Request::WRITE:

				switch (_helper.state) {
				case INIT: _write_vbas.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _vba, _count, _offset, _tag); break;
				case SB_CONTROL_REQ: progress |= _write_vbas.execute(attr.sb_control, attr.vbd, attr.client_data, attr.block_io, attr.free_tree, attr.meta_tree, attr.crypto); break;
				case SB_CONTROL_REQ_SUCCEEDED: _helper.mark_succeeded(progress); break;
				default: break;
				}
				break;

			case Request::SYNC:

				switch(_helper.state) {
				case INIT: _sync_sb_control.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress); break;
				case SB_CONTROL_REQ: progress |= _sync_sb_control.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED: _helper.mark_succeeded(progress); break;
				default: break;
				}
				break;

			case Request::DEINITIALIZE:

				switch(_helper.state) {
				case INIT: _deinit_sb_control.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress); break;
				case SB_CONTROL_REQ: progress |= _deinit_sb_control.execute(attr.sb_control, attr.block_io, attr.crypto, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED: _helper.mark_succeeded(progress); break;
				default: break;
				}
				break;

			case Request::CREATE_SNAPSHOT:

				switch(_helper.state) {
				case INIT: _create_snap.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _gen); break;
				case SB_CONTROL_REQ: progress |= _create_snap.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED: _helper.mark_succeeded(progress); break;
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

		Operation op() const { return _op; }

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
		Superblock::State sb_state() const { return _sb_state; }
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
			bool progress = false;
			bool head_complete = false;
			_schedule.with_head([&] (Request &head) {
				progress |= head.execute(attr);
				head_complete = head.complete();
			});
			if (head_complete)
				_schedule.remove_head();

			return progress;
		}

		static constexpr char const *name() { return "request_scheduler"; }
};


class Tresor::Initializing_request_scheduler
{
	private:

			enum State { INIT_TRESOR, INIT_TRESOR_SUCCEEDED, INIT_TRESOR_FAILED };

			State _state { INIT_TRESOR };
			bool _init_tresor_success { };
			Generation _init_tresor_gen { };
			Constructible<Request> _init_tresor { };
			Request_scheduler _scheduler { };

	public:

		Initializing_request_scheduler()
		{
			_init_tresor.construct(Request::INITIALIZE, 0, 0, 0, 0, 0, _init_tresor_gen, _init_tresor_success);
			_scheduler.add_request(*_init_tresor);
		}

		void add_request(Request &req) { _scheduler.add_request(req); }

		bool execute(Request::Execute_attr const &attr)
		{
			bool progress = false;
			switch(_state) {
			case INIT_TRESOR:
			{
				progress |= _scheduler.execute(attr);
				if (_init_tresor->complete()) {
					if (VERBOSE_MODULE_COMMUNICATION)
						log(name(), " <--", *_init_tresor, "-- ", Request_scheduler::name());

					if (!_init_tresor->success()) {
						error("initializing_scheduler: initialize tresor failed");
						_state = INIT_TRESOR_FAILED;
						progress = true;
					} else {

						switch (_init_tresor->sb_state()) {
						case Superblock::NORMAL: break;
						default: ASSERT_NEVER_REACHED;
						}
						_state = INIT_TRESOR_SUCCEEDED;
						progress = true;
					}
					_init_tresor.destruct();
				}
				break;
			}
			case INIT_TRESOR_SUCCEEDED:

				progress |= _scheduler.execute(attr);
				break;

			default: break;
			}
			return progress;
		}

		static constexpr char const *name() { return "initializing_request_scheduler"; }
};

#endif /* _TRESOR__REQUEST_POOL_H_ */
