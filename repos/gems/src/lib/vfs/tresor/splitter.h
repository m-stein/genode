/*
 * \brief  Module for splitting unaligned/uneven I/O requests
 * \author Martin Stein
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
#include <tresor/request_scheduler.h>

namespace Tresor {

	struct Lookup_buffer : Genode::Interface
	{
		virtual Block const &source_buffer(Virtual_block_address) = 0;
		virtual Block &destination_buffer(Virtual_block_address) = 0;
	};

	class Splitter;
}

struct Tresor::Splitter : Noncopyable, public Lookup_buffer
{
	public:

		class Read : Noncopyable
		{
			public:

				using Module = Splitter;

				struct Attr
				{
					Request_offset const in_off;
					Generation const in_gen;
					Byte_range_ptr const in_buf;

					Attr(Attr const &attr) : in_off(attr.in_off), in_gen(attr.in_gen), in_buf(attr.in_buf.start, attr.in_buf.num_bytes) { }
				};

				struct Execute_attr
				{
					Request_scheduler &scheduler;
					Request::Execute_attr const request_attr;
				};

			private:

				enum State {
					INIT, COMPLETE, READ_FIRST_BLOCK, READ_FIRST_BLOCK_SUCCEEDED, READ_LAST_BLOCK, READ_LAST_BLOCK_SUCCEEDED,
					READ_MIDDLE_BLOCKS, READ_MIDDLE_BLOCKS_SUCCEEDED };

				Request_helper<Read, State> _helper;
				Attr const _attr;
				addr_t _curr_off { };
				addr_t _curr_buf_addr { };
				Block _blk  { };
				Generation _gen { };
				Constructible<Request> _request { };

				Virtual_block_address _curr_vba() const { return (Virtual_block_address)(_curr_off / BLOCK_SIZE); }

				void _generate_request(State target_state, Request_scheduler &scheduler, bool &progress)
				{
					Number_of_blocks num_blocks =
						target_state == READ_MIDDLE_BLOCKS ? _num_remaining_bytes() / BLOCK_SIZE : 1;

					_request.construct(Request::READ, _curr_vba(), 0, num_blocks, 0, _gen);
					scheduler.add_request(*_request);
					_helper.state = target_state;
					progress = true;
				}

				bool _execute_request(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.scheduler.execute(attr.request_attr);
					if (_request->complete()) {
						if (_request->success())
							_helper.generated_req_succeeded(succeeded_state, progress);
						else
							_helper.generated_req_failed(progress);
					}
					return progress;
				}

				addr_t _curr_buf_off() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf.num_bytes);
					return _curr_off - _attr.in_off;
				}

				addr_t _num_remaining_bytes() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf.num_bytes);
					return _attr.in_off + _attr.in_buf.num_bytes - _curr_off;
				}

				void _advance_curr_off(size_t advance, Request_scheduler &scheduler, bool &progress)
				{
					_curr_off += advance;
					if (!_num_remaining_bytes()) {
						_helper.mark_succeeded(progress);
					} else if (_curr_off % BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_request(READ_FIRST_BLOCK, scheduler, progress);
					} else if (_num_remaining_bytes() < BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_request(READ_LAST_BLOCK, scheduler, progress);
					} else {
						_curr_buf_addr = (addr_t)_attr.in_buf.start + _curr_buf_off();
						_generate_request(READ_MIDDLE_BLOCKS, scheduler, progress);
					}
				}

			public:

				Read(Attr const &attr) : _helper(*this), _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "read"); }

				bool execute(Execute_attr const &attr)
				{
					bool progress = false;
					switch (_helper.state) {
					case INIT:

						_gen = _attr.in_gen;
						_advance_curr_off(_attr.in_off, attr.scheduler, progress);
						break;

					case READ_FIRST_BLOCK: progress |= _execute_request(READ_FIRST_BLOCK_SUCCEEDED, attr); break;
					case READ_FIRST_BLOCK_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						memcpy(_attr.in_buf.start, (void *)((addr_t)&_blk + num_outside_bytes), num_inside_bytes);
						_advance_curr_off(num_inside_bytes, attr.scheduler, progress);
						break;
					}
					case READ_MIDDLE_BLOCKS: progress |= _execute_request(READ_MIDDLE_BLOCKS_SUCCEEDED, attr); break;
					case READ_MIDDLE_BLOCKS_SUCCEEDED:

						_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, attr.scheduler, progress);
						break;

					case READ_LAST_BLOCK: progress |= _execute_request(READ_LAST_BLOCK_SUCCEEDED, attr); break;
					case READ_LAST_BLOCK_SUCCEEDED:

						memcpy((void *)((addr_t)_attr.in_buf.start + _curr_buf_off()), &_blk, _num_remaining_bytes());
						_advance_curr_off(_num_remaining_bytes(), attr.scheduler, progress);
						break;

					default: break;
					}
					return progress;
				}

				Block &destination_buffer(Virtual_block_address vba)
				{
					return *(Block *)(_curr_buf_addr + (vba - _curr_vba()) * BLOCK_SIZE);
				}

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Write : Noncopyable
		{
			public:

				using Module = Splitter;

				struct Attr
				{
					Request_offset const in_off;
					Generation const in_gen;
					Byte_range_ptr const in_buf;

					Attr(Attr const &attr) : in_off(attr.in_off), in_gen(attr.in_gen), in_buf(attr.in_buf.start, attr.in_buf.num_bytes) { }
				};

				struct Execute_attr
				{
					Request_scheduler &scheduler;
					Request::Execute_attr const request_attr;
				};

			private:

				enum State {
					INIT, COMPLETE, READ_FIRST_BLOCK, READ_FIRST_BLOCK_SUCCEEDED, READ_LAST_BLOCK, READ_LAST_BLOCK_SUCCEEDED,
					WRITE_FIRST_BLOCK, WRITE_FIRST_BLOCK_SUCCEEDED, WRITE_LAST_BLOCK, WRITE_LAST_BLOCK_SUCCEEDED,
					WRITE_MIDDLE_BLOCKS, WRITE_MIDDLE_BLOCKS_SUCCEEDED };

				Request_helper<Write, State> _helper;
				Attr const _attr;
				addr_t _curr_off { };
				addr_t _curr_buf_addr { };
				Block _blk  { };
				Generation _gen { };
				Constructible<Request> _request { };

				Virtual_block_address _curr_vba() const { return (Virtual_block_address)(_curr_off / BLOCK_SIZE); }

				addr_t _curr_buf_off() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf.num_bytes);
					return _curr_off - _attr.in_off;
				}

				addr_t _num_remaining_bytes() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf.num_bytes);
					return _attr.in_off + _attr.in_buf.num_bytes - _curr_off;
				}

				void _generate_request(State target_state, Request_scheduler &scheduler, bool &progress)
				{
					Number_of_blocks num_blocks =
						target_state == WRITE_MIDDLE_BLOCKS ? _num_remaining_bytes() / BLOCK_SIZE : 1;

					Request::Operation op;
					switch (target_state) {
					case READ_FIRST_BLOCK:
					case READ_LAST_BLOCK: op = Request::READ; break;
					case WRITE_FIRST_BLOCK:
					case WRITE_MIDDLE_BLOCKS:
					case WRITE_LAST_BLOCK: op = Request::WRITE; break;
					default: ASSERT_NEVER_REACHED;
					}
					_request.construct(op, _curr_vba(), 0, num_blocks, 0, _gen);
					scheduler.add_request(*_request);
					_helper.state = target_state;
					progress = true;
				}

				void _advance_curr_off(size_t advance, Request_scheduler &scheduler, bool &progress)
				{
					_curr_off += advance;
					if (!_num_remaining_bytes()) {
						_helper.mark_succeeded(progress);
					} else if (_curr_off % BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_request(READ_FIRST_BLOCK, scheduler, progress);
					} else if (_num_remaining_bytes() < BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_request(READ_LAST_BLOCK, scheduler, progress);
					} else {
						_curr_buf_addr = (addr_t)_attr.in_buf.start + _curr_buf_off();
						_generate_request(WRITE_MIDDLE_BLOCKS, scheduler, progress);
					}
				}

				bool _execute_request(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.scheduler.execute(attr.request_attr);
					if (_request->complete()) {
						if (_request->success())
							_helper.generated_req_succeeded(succeeded_state, progress);
						else
							_helper.generated_req_failed(progress);
					}
					return progress;
				}

			public:

				Write(Attr const &attr) : _helper(*this), _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "write"); }

				bool execute(Execute_attr const &attr)
				{
					bool progress = false;
					switch (_helper.state) {
					case INIT:

						_gen = _attr.in_gen;
						_advance_curr_off(_attr.in_off, attr.scheduler, progress);
						break;

					case READ_FIRST_BLOCK_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						memcpy((void *)((addr_t)&_blk + num_outside_bytes), _attr.in_buf.start, num_inside_bytes);
						_curr_buf_addr = (addr_t)&_blk;
						_generate_request(WRITE_FIRST_BLOCK, attr.scheduler, progress);
						break;
					}
					case WRITE_FIRST_BLOCK: progress |= _execute_request(WRITE_FIRST_BLOCK_SUCCEEDED, attr); break;
					case WRITE_FIRST_BLOCK_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						_advance_curr_off(num_inside_bytes, attr.scheduler, progress);
						break;
					}
					case WRITE_MIDDLE_BLOCKS: progress |= _execute_request(WRITE_MIDDLE_BLOCKS_SUCCEEDED, attr); break;
					case WRITE_MIDDLE_BLOCKS_SUCCEEDED:

						_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, attr.scheduler, progress);
						break;

					case READ_LAST_BLOCK: progress |= _execute_request(READ_LAST_BLOCK_SUCCEEDED, attr); break;
					case READ_LAST_BLOCK_SUCCEEDED:

						memcpy(&_blk, (void *)((addr_t)_attr.in_buf.start + _curr_buf_off()), _num_remaining_bytes());
						_curr_buf_addr = (addr_t)&_blk;
						_generate_request(WRITE_LAST_BLOCK, attr.scheduler, progress);
						break;

					case WRITE_LAST_BLOCK: progress |= _execute_request(WRITE_LAST_BLOCK_SUCCEEDED, attr); break;
					case WRITE_LAST_BLOCK_SUCCEEDED: _advance_curr_off(_num_remaining_bytes(), attr.scheduler, progress); break;
					default: break;
					}
					return progress;
				}

				Block const &source_buffer(Virtual_block_address vba)
				{
					return *(Block *)(_curr_buf_addr + (vba - _curr_vba()) * BLOCK_SIZE);
				}

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

	private:

		Read *_read_ptr { };
		Write *_write_ptr { };

	public:

		bool execute(Read &req, Read::Execute_attr const &attr)
		{
			if (!_read_ptr && !_write_ptr)
				_read_ptr = &req;

			if (_read_ptr != &req)
				return false;

			bool progress = req.execute(attr);
			if (req.complete())
				_read_ptr = nullptr;

			return progress;
		}

		bool execute(Write &req, Write::Execute_attr const &attr)
		{
			if (!_write_ptr && !_write_ptr)
				_write_ptr = &req;

			if (_write_ptr != &req)
				return false;

			bool progress = req.execute(attr);
			if (req.complete())
				_write_ptr = nullptr;

			return progress;
		}

		Block const &source_buffer(Virtual_block_address vba) override
		{
			ASSERT(_write_ptr);
			return _write_ptr->source_buffer(vba);
		}

		Block &destination_buffer(Virtual_block_address vba) override
		{
			ASSERT(_read_ptr);
			return _read_ptr->destination_buffer(vba);
		}
};


#endif /* _TRESOR__IO_SPLITTER_H_ */
