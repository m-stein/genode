/*
 * \brief  Module for splitting unaligned/uneven I/O requests
 * \author Josef Soentgen
 * \date   2023-09-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__IO_SPLITTER_H_
#define _TRESOR__IO_SPLITTER_H_

/* tresor includes */
#include <tresor/request_pool.h>

namespace Tresor {

	struct Lookup_buffer : Genode::Interface
	{
		virtual void const *write_buffer(Genode::uint64_t, Genode::uint64_t) = 0;
		virtual void       *read_buffer(Genode::uint64_t,  Genode::uint64_t) = 0;
	};

	class Splitter_request;
	class Splitter_channel;
	class Splitter;

} /* namespace Tresor */


class Tresor::Splitter_request : public Tresor::Module_request
{
	private:

		NONCOPYABLE(Splitter_request);

		friend class Splitter_channel;

	public:

		enum class Operation { READ, WRITE };

		static char const *op_to_string(Operation op)
		{
			switch (op) {
			case Operation::READ:  return "READ";
			case Operation::WRITE: return "WRITE";
			}
			return "UNKNOWN";
		}

	private:

		Operation              const  _op;
		Genode::uint64_t       const  _offset;
		Tresor::Key_id         const  _key_id;
		Tresor::Generation     const  _gen;
		bool                         &_success;

		char   *_buffer_start     { nullptr };
		size_t  _buffer_num_bytes { 0 };

	public:

		void print(Genode::Output &out) const override
		{
			Genode::print(out, "op: ", op_to_string(_op), " "
			                   "offset: ", _offset, " "
			                   "key_id: ", _key_id, " "
			                   "gen: ", _gen, " "
			                   "start: ", (void*)_buffer_start, " "
			                   "num_bytes: ", _buffer_num_bytes);
		}

		Splitter_request(Module_id                     src_module_id,
		                 Module_channel_id             src_chan_id,
		                 Operation                     op,
		                 bool                         &success,
		                 Genode::uint64_t              offset,
		                 char                         *buffer_start,
		                 size_t                        buffer_num_bytes,
		                 Key_id                        key_id,
		                 Generation                    gen)
		:
			Module_request { src_module_id, src_chan_id, SPLITTER },
			_op { op }, _offset { offset }, _key_id { key_id }, _gen { gen },
			_success { success },
			_buffer_start { buffer_start }, _buffer_num_bytes { buffer_num_bytes }
		{ }
};


class Tresor::Splitter_channel : public Tresor::Module_channel
{
	private:

		NONCOPYABLE(Splitter_channel);

		/*
		 * The Splitter module chops each I/O request into aligned
		 * and properly sized requests. The 'PRE_REQUEST' state is
		 * entered when the I/O requests does not start at a BLOCK_SIZE
		 * boundary while the 'POST_REQUEST' request deals with smaller
		 * than BLOCK_SIZE requests. All REQUEST states may lead to the
		 * COMPLETE state.
		 *
		 * Depending of the nature of the request the flow is as follows:
		 *
		 * READ:  [PRE ->]              REQUEST [-> POST]               -> COMPLETE
		 * WRITE: [PRE -> PRE_WRITE ->] REQUEST [-> POST -> POST_WRITE] -> COMPLETE
		 */
		enum State : State_uint {
			IDLE,
			PENDING, REQUEST, /* normal request */
			PRE_REQUEST_PENDING, PRE_REQUEST, /* unaligned request */
			PRE_REQUEST_WRITE_PENDING, PRE_REQUEST_WRITE,
			POST_REQUEST_PENDING, POST_REQUEST, /* uneven request */
			POST_REQUEST_WRITE_PENDING, POST_REQUEST_WRITE,
			COMPLETE };

		static char const *state_to_string(State state)
		{
			switch (state) {
			case State::IDLE:                       return "IDLE";
			case State::PENDING:                    return "PENDING";
			case State::REQUEST:                    return "REQUEST";
			case State::PRE_REQUEST_PENDING:        return "PRE_REQUEST_PENDING";
			case State::PRE_REQUEST:                return "PRE_REQUEST";
			case State::PRE_REQUEST_WRITE_PENDING:  return "PRE_REQUEST_WRITE_PENDING";
			case State::PRE_REQUEST_WRITE:          return "PRE_REQUEST_WRITE";
			case State::POST_REQUEST_PENDING:       return "POST_REQUEST_PENDING";
			case State::POST_REQUEST:               return "POST_REQUEST";
			case State::POST_REQUEST_WRITE_PENDING: return "POST_REQUEST_PENDING";
			case State::POST_REQUEST_WRITE:         return "POST_REQUEST";
			case State::COMPLETE:                   return "COMPLETE";
			}
			return "UNKNOWN";
		}

		State _state { IDLE };
		Splitter_request *_req_ptr { nullptr };

		Genode::uint64_t _offset      { 0 };
		size_t           _total_bytes { 0 };

		Genode::uint32_t      _count       { 0 };
		Virtual_block_address _vba         { INVALID_VBA };

		/* used temporarily for lopsided requests */
		Tresor::Block _block_data  { };

		void _reset()
		{
			state(Splitter_channel::IDLE);
			_req_ptr     = nullptr;
			_offset      = 0;
			_total_bytes = 0;
			_vba         = INVALID_VBA;
			_count       = 0;
			Genode::memset((void*)&_block_data,  0, sizeof(_block_data));
		}

		void *_calculate_data_ptr(Virtual_block_address);

		void _handle_io(bool &);

		void _generated_req_completed_read();
		void _generated_req_completed_write();

		/************************
		 ** Module_channel API **
		 ************************/

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override;

	public:

		void print(Genode::Output &out) const
		{
			Genode::print(out, "state: ", state_to_string(_state), " "
			                   "req_ptr: ", (void*)_req_ptr, " "
			                   "offset: ", _offset, " "
			                   "total_bytes: ", _total_bytes, " "
			                   "vba: ", _vba, " "
			                   "count: ", _count);
		}

		Splitter_channel(Module_channel_id id)
		:
			Module_channel { SPLITTER, id }
		{ }

		void state(State state)
		{
			ASSERT(_state != state || _state == State::IDLE);

			_state = state;
		}

		State state() const { return _state; }

		void execute(bool &progress);

		void *query_data(Virtual_block_address vba);
};


class Tresor::Splitter : public Tresor::Module,
                         public Tresor::Lookup_buffer
{
	private:

		NONCOPYABLE(Splitter);

		using Channel = Splitter_channel;

		enum { NUM_CHANNELS = 1 };

		Constructible<Channel> _channels[NUM_CHANNELS] { };

	public:

		Splitter();

		void execute(bool &) override;

		/*****************************
		 ** Lookup_buffer interface **
		 *****************************/

		void const *write_buffer(Genode::uint64_t tag, Genode::uint64_t vba) override
		{
			void const *ptr = nullptr;

			with_channel<Splitter_channel>(tag, [&] (Splitter_channel &chan) {
				ptr = (void const*)chan.query_data(vba);
			});
			return ptr;
		}

		void *read_buffer(Genode::uint64_t tag, Genode::uint64_t vba) override
		{
			void *ptr = nullptr;

			with_channel<Splitter_channel>(tag, [&] (Splitter_channel &chan) {
				ptr = (void *)chan.query_data(vba);
			});
			return ptr;
		}
};


#endif /* _TRESOR__IO_SPLITTER_H_ */
