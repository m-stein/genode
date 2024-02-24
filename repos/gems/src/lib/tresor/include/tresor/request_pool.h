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

class Tresor::Request : public Module_request, private List<Tresor::Request>::Element
{
	NONCOPYABLE(Request);

	friend class Request_pool_channel;
	friend class Request_scheduler;
	friend class List<Tresor::Request>;

	public:

		enum Operation {
			READ, WRITE, SYNC, CREATE_SNAPSHOT, DISCARD_SNAPSHOT, REKEY, EXTEND_VBD,
			EXTEND_FT, RESUME_REKEYING, DEINITIALIZE, INITIALIZE, };

	private:

		Operation _op;
		Virtual_block_address const _vba;
		Request_offset const _offset;
		Number_of_blocks const _count;
		Key_id const _key_id;
		Request_tag const _tag;
		Generation &_gen;
		bool &_success;

	public:

		static char const *op_to_string(Operation);

		Request(Module_id, Module_channel_id, Operation, Virtual_block_address, Request_offset,
		        Number_of_blocks, Key_id, Request_tag, Generation &, bool &);

		void print(Output &) const override;
};

class Tresor::Request_pool_channel : public Module_channel
{
	public:

		using Module = Request_pool;

	private:

		enum State : State_uint {
			INVALID, REQ_SUBMITTED, REQ_RESUMED, REQ_GENERATED, PREPONED_REQUESTS_COMPLETE,
			EXTEND_VBD, EXTEND_VBD_SUCCEEDED, EXTEND_FT, EXTEND_FT_SUCCEEDED, TREE_EXTENSION_STEP_SUCCEEDED, FORWARD_TO_SB_CTRL_SUCCEEDED, READ_VBAS, READ_VBAS_SUCCEEDED, WRITE_VBAS, WRITE_VBAS_SUCCEEDED,
			ACCESS_VBA_AT_SB_CTRL_SUCCEEDED,
			STATE_REKEY, STATE_REKEY_SUCCEEDED, INIT_SB_CONTROL, INIT_SB_CONTROL_SUCCEEDED, DEINITIALIZE_SB_CTRL_SUCCEEDED, REQ_COMPLETE,
			SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED};

		State _state { INVALID };
		Superblock::State _sb_state { Superblock::INVALID };
		uint32_t _num_requests_preponed { 0 };
		bool _request_finished { false };
		bool _generated_req_success { false };
		Request_pool_channel_queue &_chan_queue;
		Request *_req_ptr { nullptr };
		union {
			Generatable_request<Request_pool_channel, State, Superblock_control::Read_vbas> _read_vbas;
			Generatable_request<Request_pool_channel, State, Superblock_control::Write_vbas> _write_vbas;
			Generatable_request<Request_pool_channel, State, Superblock_control::Discard_snapshot> _discard_snap;
			Generatable_request<Request_pool_channel, State, Superblock_control::Create_snapshot> _create_snap;
			Generatable_request<Request_pool_channel, State, Superblock_control::Initialize> _init_sb_control;
			Generatable_request<Request_pool_channel, State, Superblock_control::Deinitialize> _deinit_sb_control;
			Generatable_request<Request_pool_channel, State, Superblock_control::Synchronize> _sync_sb_control;
			Generatable_request<Request_pool_channel, State, Superblock_control::Rekey> _rekey;
			Generatable_request<Request_pool_channel, State, Superblock_control::Extend_vbd> _extend_vbd;
			Generatable_request<Request_pool_channel, State, Superblock_control::Extend_free_tree> _extend_ft;
		};

		NONCOPYABLE(Request_pool_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &req) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _mark_req_successful(bool &);

		void _reset();

		void _try_prepone_requests(bool &);

		void _resume_request(bool &, Request::Operation);

	public:

		~Request_pool_channel() { }

		Request_pool_channel(Module_channel_id id, Request_pool_channel_queue &chan_queue) : Module_channel(REQUEST_POOL, id), _chan_queue(chan_queue) { }

		void execute(Superblock_control &, Trust_anchor &, Virtual_block_device &, Client_data_interface &, Block_io &, Free_tree &, Meta_tree &, Crypto &, bool &);

		void generated_req_failed(bool &progress);

		void generated_req_succeeded(State target_state, bool &progress)
		{
			_state = target_state;
			progress = true;
		}

		void req_generated(State target_state, bool &progress)
		{
			_state = target_state;
			progress = true;
		}
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


class Tresor::Request_pool : public Module
{
	NONCOPYABLE(Request_pool);

	private:

		using Channel = Request_pool_channel;

		enum { NUM_CHANNELS = Request_pool_channel_queue::NUM_SLOTS };

		bool _init_success { false };
		Generation _init_gen { INVALID_GENERATION };
		Request _init_req { INVALID_MODULE_ID, INVALID_MODULE_CHANNEL_ID, Request::INITIALIZE, 0, 0, 0, 0, 0, _init_gen, _init_success };
		Constructible<Channel> _channels[NUM_CHANNELS] { };
		Request_pool_channel_queue _chan_queue { };
		Superblock_control &_sb_control;
		Trust_anchor &_trust_anchor;
		Virtual_block_device &_vbd;
		Client_data_interface &_client_data;
		Block_io &_block_io;
		Free_tree &_free_tree;
		Meta_tree &_meta_tree;
		Crypto &_crypto;

	public:

		void execute(bool &) override;

		Request_pool(Superblock_control &, Trust_anchor &, Virtual_block_device &, Client_data_interface &, Block_io &, Free_tree &, Meta_tree &, Crypto &);

		static constexpr char const *name() { return "request_pool"; }
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

				void insert(Request &request)
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
};

#endif /* _TRESOR__REQUEST_POOL_H_ */
