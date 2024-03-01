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
	class Request_scheduler;
	class Initializing_request_scheduler;
}

class Tresor::Request : Noncopyable, private List<Tresor::Request>::Element
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

		enum State { INIT, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, COMPLETE };

		using Helper = Request_helper<Tresor::Request, State>;

		Operation _op;
		Virtual_block_address const _vba;
		Request_offset const _offset;
		Number_of_blocks const _count;
		Request_tag const _tag;
		Generation &_gen;
		Helper _helper;
		Superblock::State _sb_state { Superblock::INVALID };
		bool _request_finished { false };
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

			case Request::DISCARD_SNAPSHOT:

				switch(_helper.state) {
				case INIT: _discard_snap.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _gen); break;
				case SB_CONTROL_REQ: progress |= _discard_snap.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED: _helper.mark_succeeded(progress); break;
				default: break;
				}
				break;

			case Request::REKEY:

				switch(_helper.state) {
				case INIT: _rekey.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _request_finished); break;
				case SB_CONTROL_REQ: progress |= _rekey.execute(attr.sb_control, attr.vbd, attr.free_tree, attr.meta_tree, attr.block_io, attr.crypto, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED:

					if (_request_finished)
						_helper.mark_succeeded(progress);
					else {
						_helper.state = INIT;
						progress = true;
					}
					break;

				default: break;
				}
				break;

			case Request::EXTEND_FT:

				switch(_helper.state) {
				case INIT: _extend_ft.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _count, _request_finished); break;
				case SB_CONTROL_REQ: progress |= _extend_ft.execute(attr.sb_control, attr.free_tree, attr.meta_tree, attr.block_io, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED:

					if (_request_finished)
						_helper.mark_succeeded(progress);
					else {
						_helper.state = INIT;
						progress = true;
					}
					break;

				default: break;
				}
				break;

			case Request::EXTEND_VBD:

				switch(_helper.state) {
				case INIT: _extend_vbd.generate(_helper, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, _count, _request_finished); break;
				case SB_CONTROL_REQ: progress |= _extend_vbd.execute(attr.sb_control, attr.vbd, attr.free_tree, attr.meta_tree, attr.block_io, attr.trust_anchor); break;
				case SB_CONTROL_REQ_SUCCEEDED:

					if (_request_finished)
						_helper.mark_succeeded(progress);
					else {
						_helper.state = INIT;
						progress = true;
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
		        Number_of_blocks, Request_tag, Generation &);

		~Request() { }

		void print(Output &) const;

		Operation op() const { return _op; }

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
		Superblock::State sb_state() const { return _sb_state; }
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

				template <typename CAN_YIELD_TO_FN>
				void try_yield_head(CAN_YIELD_TO_FN && can_yield_to)
				{
					Request *head = _list.first();
					if (!head)
						return;

					Request *next = head->List<Request>::Element::_next;
					if (!next || !can_yield_to(*next))
						return;

					remove_head();
					_list.insert(head, next);
				}
		};

		Schedule _schedule { };

	public:

		void add_request(Request &req) { _schedule.add_tail(req); }

		bool execute(Request::Execute_attr const &attr)
		{
			bool progress = false;
			_schedule.with_head([&] (Request &head) {
				progress |= head.execute(attr);
				if (head.complete())
					_schedule.remove_head();
				else
					switch (head._op) {
					case Request::REKEY:
					case Request::EXTEND_VBD:
					case Request::EXTEND_FT:
						if (head._helper.state != Request::INIT)
							break;

						_schedule.try_yield_head([&] (Request &to_req) {
							switch (to_req.op()) {
							case Request::READ:
							case Request::WRITE:
							case Request::SYNC:
							case Request::DISCARD_SNAPSHOT: return true;
							default: return false;
							}
						});
						break;
					default: break;
					}
			});
			return progress;
		}

		static constexpr char const *name() { return "request_scheduler"; }
};


class Tresor::Initializing_request_scheduler : Noncopyable
{
	private:

		enum State { INIT_TRESOR, INIT_TRESOR_SUCCEEDED, INIT_TRESOR_FAILED };

		State _state { INIT_TRESOR };
		Generation _init_tresor_gen { };
		Constructible<Request> _init_tresor { };
		Request_scheduler _scheduler { };

	public:

		Initializing_request_scheduler()
		{
			_init_tresor.construct(Request::INITIALIZE, 0, 0, 0, 0, _init_tresor_gen);
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
