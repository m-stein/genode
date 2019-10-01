/*
 * \brief  Block session testing - compare test
 * \author Josef Soentgen
 * \date   2019-10-01
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TEST_COMPARE_H_
#define _TEST_COMPARE_H_

namespace Test { struct Compare; }

namespace Util {
	
}


/*
 * Compare operation test
 *
 * This test first writes and then reads it a request. It compares
 * the expected checksum of the request with the actual one.
 */
struct Test::Compare : Test_base
{
	block_number_t _start   = _node.attribute_value("start",  0u);
	size_t   const _size    = _node.attribute_value("size",   Number_of_bytes());
	size_t   const _length  = _node.attribute_value("length", Number_of_bytes());

	Genode::uint32_t _jobs = 0;

	using Test_base::Test_base;

	void produce_write_content(Job &job, off_t offset,
	                           char *dst, size_t length) override
	{
		Genode::uint8_t const pattern_byte = job.operation().block_number & 0xff;

		if (length < _scratch_buffer.size) {
			Genode::memset(_scratch_buffer.base, pattern_byte, length);
		}

		Test_base::produce_write_content(job, offset, dst, length);
	}

	void consume_read_result(Job &job, off_t offset,
	                         char const *src, size_t length) override
	{
		Test_base::consume_read_result(job, offset, src, length);
	}

	void completed(Job &job, bool success) override
	{
		if (job.operation().type == Block::Operation::Type::READ) {
			Block::block_number_t const bn = job.operation().block_number;

			Genode::uint8_t  const pattern_byte = bn & 0xff;
			Genode::uint64_t const check_sum    = pattern_byte * _size;

			Genode::uint64_t sum = 0;
			Genode::uint8_t const *src = reinterpret_cast<Genode::uint8_t*>(_scratch_buffer.base);
			for (size_t i = 0; i < _size; i++) {
				sum += src[i];
			}

			if (sum != check_sum) {
				Genode::error("comparing ", bn, " failed, check-sum mismatch - expected: ",
				               check_sum, " got: ", sum);
				success = false;
			}
		}

		Test_base::completed(job, success);
	}

	void _init() override
	{
		if (_size > _scratch_buffer.size) {
			error("request size exceeds scratch buffer size");
			throw Constructing_test_failed();
		}

		if (_info.block_size > _size || (_size % _info.block_size) != 0) {
			error("request size invalid");
			throw Constructing_test_failed();
		}

		if (!_copy) {
			error("compare test relies on copy attribute being true");
			throw Constructing_test_failed();
		}

		_size_in_blocks   = _size / _info.block_size;
		_length_in_blocks = _length / _info.block_size;
		_job_cnt          = (_length_in_blocks / _size_in_blocks) * 2;
	}

	void _spawn_job() override
	{
		if (_jobs >= _job_cnt)
			return;

		_jobs++;

		bool const jobs_odd = _jobs & 1;

		Block::Operation::Type const op_type = jobs_odd ? Block::Operation::Type::WRITE
		                                                : Block::Operation::Type::READ;

		Block::Operation const operation { .type         = op_type,
		                                   .block_number = _start,
		                                   .count        = _size_in_blocks };

		new (_alloc) Job(*_block, operation, _jobs);

		if (!jobs_odd)
			_start += _size_in_blocks;
	}

	Result result() override
	{
		return Result(_success, _end_time - _start_time,
		              _bytes, _rx, _tx, _size, _info.block_size, _triggered);
	}

	char const *name() const override { return "compare"; }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, name(), " ",
		                   "start:",  _start,  " "
		                   "size:",   _size,   " "
		                   "length:", _length, " "
		                   "copy:",   _copy,   " "
		                   "batch:",  _batch);
	}
};

#endif /* _TEST_COMPARE_H_ */
