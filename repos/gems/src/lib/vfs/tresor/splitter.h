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
#include <tresor/superblock_control.h>

namespace Tresor { class Splitter; }

struct Tresor::Splitter : Noncopyable
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
					Superblock_control &sb_control;
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Crypto &crypto;
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
				Constructible<Superblock_control::Read> _read { };

				Virtual_block_address _curr_vba() const { return (Virtual_block_address)(_curr_off / BLOCK_SIZE); }

				void _generate_read(State target_state, bool &progress)
				{
					Number_of_blocks num_blocks =
						target_state == READ_MIDDLE_BLOCKS ? _num_remaining_bytes() / BLOCK_SIZE : 1;

					_read.construct(_curr_vba(), 0, num_blocks, 0, _gen);
					_helper.state = target_state;
					progress = true;
				}

				bool _execute_read(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.sb_control.execute(*_read, attr.vbd, attr.client_data, attr.block_io, attr.crypto);
					if (_read->complete()) {
						if (_read->success())
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

				void _advance_curr_off(size_t advance, bool &progress)
				{
					_curr_off += advance;
					if (!_num_remaining_bytes()) {
						_helper.mark_succeeded(progress);
					} else if (_curr_off % BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_read(READ_FIRST_BLOCK, progress);
					} else if (_num_remaining_bytes() < BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_read(READ_LAST_BLOCK, progress);
					} else {
						_curr_buf_addr = (addr_t)_attr.in_buf.start + _curr_buf_off();
						_generate_read(READ_MIDDLE_BLOCKS, progress);
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
						_advance_curr_off(_attr.in_off, progress);
						break;

					case READ_FIRST_BLOCK: progress |= _execute_read(READ_FIRST_BLOCK_SUCCEEDED, attr); break;
					case READ_FIRST_BLOCK_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						memcpy(_attr.in_buf.start, (void *)((addr_t)&_blk + num_outside_bytes), num_inside_bytes);
						_advance_curr_off(num_inside_bytes, progress);
						break;
					}
					case READ_MIDDLE_BLOCKS: progress |= _execute_read(READ_MIDDLE_BLOCKS_SUCCEEDED, attr); break;
					case READ_MIDDLE_BLOCKS_SUCCEEDED:

						_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, progress);
						break;

					case READ_LAST_BLOCK: progress |= _execute_read(READ_LAST_BLOCK_SUCCEEDED, attr); break;
					case READ_LAST_BLOCK_SUCCEEDED:

						memcpy((void *)((addr_t)_attr.in_buf.start + _curr_buf_off()), &_blk, _num_remaining_bytes());
						_advance_curr_off(_num_remaining_bytes(), progress);
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
					Superblock_control &sb_control;
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Crypto &crypto;
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
				Constructible<Superblock_control::Read> _read { };
				Constructible<Superblock_control::Write> _write { };

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

				void _generate_sb_control_request(State target_state, bool &progress)
				{
					Number_of_blocks num_blocks =
						target_state == WRITE_MIDDLE_BLOCKS ? _num_remaining_bytes() / BLOCK_SIZE : 1;

					switch (target_state) {
					case READ_FIRST_BLOCK:
					case READ_LAST_BLOCK: _read.construct(_curr_vba(), 0, num_blocks, 0, _gen); break;
					case WRITE_FIRST_BLOCK:
					case WRITE_MIDDLE_BLOCKS:
					case WRITE_LAST_BLOCK: _write.construct(_curr_vba(), 0, num_blocks, 0, _gen); break; 
					default: ASSERT_NEVER_REACHED;
					}
					_helper.state = target_state;
					progress = true;
				}

				void _advance_curr_off(size_t advance, bool &progress)
				{
					_curr_off += advance;
					if (!_num_remaining_bytes()) {
						_helper.mark_succeeded(progress);
					} else if (_curr_off % BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(READ_FIRST_BLOCK, progress);
					} else if (_num_remaining_bytes() < BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(READ_LAST_BLOCK, progress);
					} else {
						_curr_buf_addr = (addr_t)_attr.in_buf.start + _curr_buf_off();
						_generate_sb_control_request(WRITE_MIDDLE_BLOCKS, progress);
					}
				}

				bool _execute_read(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.sb_control.execute(*_read, attr.vbd, attr.client_data, attr.block_io, attr.crypto);
					if (_read->complete()) {
						if (_read->success())
							_helper.generated_req_succeeded(succeeded_state, progress);
						else
							_helper.generated_req_failed(progress);
					}
					return progress;
				}

				bool _execute_write(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.sb_control.execute(*_write, attr.vbd, attr.client_data, attr.block_io, attr.free_tree, attr.meta_tree, attr.crypto);
					if (_write->complete()) {
						if (_write->success())
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
						_advance_curr_off(_attr.in_off, progress);
						break;

					case READ_FIRST_BLOCK: progress |= _execute_read(READ_FIRST_BLOCK_SUCCEEDED, attr); break;
					case READ_FIRST_BLOCK_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						memcpy((void *)((addr_t)&_blk + num_outside_bytes), _attr.in_buf.start, num_inside_bytes);
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(WRITE_FIRST_BLOCK, progress);
						break;
					}
					case WRITE_FIRST_BLOCK: progress |= _execute_write(WRITE_FIRST_BLOCK_SUCCEEDED, attr); break;
					case WRITE_FIRST_BLOCK_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						_advance_curr_off(num_inside_bytes, progress);
						break;
					}
					case WRITE_MIDDLE_BLOCKS: progress |= _execute_write(WRITE_MIDDLE_BLOCKS_SUCCEEDED, attr); break;
					case WRITE_MIDDLE_BLOCKS_SUCCEEDED:

						_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, progress);
						break;

					case READ_LAST_BLOCK: progress |= _execute_read(READ_LAST_BLOCK_SUCCEEDED, attr); break;
					case READ_LAST_BLOCK_SUCCEEDED:

						memcpy(&_blk, (void *)((addr_t)_attr.in_buf.start + _curr_buf_off()), _num_remaining_bytes());
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(WRITE_LAST_BLOCK, progress);
						break;

					case WRITE_LAST_BLOCK: progress |= _execute_write(WRITE_LAST_BLOCK_SUCCEEDED, attr); break;
					case WRITE_LAST_BLOCK_SUCCEEDED: _advance_curr_off(_num_remaining_bytes(), progress); break;
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
