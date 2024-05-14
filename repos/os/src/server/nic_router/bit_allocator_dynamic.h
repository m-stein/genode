/*
 * \brief  Bit Allocator that uses a dynamically allocated RAM dataspace
 * \author Martin Stein
 * \date   2017-09-26
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BIT_ALLOCATOR_DYNAMIC_H_
#define _BIT_ALLOCATOR_DYNAMIC_H_

/* Genode includes */
#include <base/allocator.h>

/* local includes */
#include <assertion.h>

namespace Genode {

	class Bit_array_dynamic;
	class Bit_allocator_dynamic;
}


class Genode::Bit_array_dynamic
{
	protected:

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE
		};

	private:

		unsigned _bit_cnt;
		unsigned _word_cnt;
		addr_t  *_words;

		addr_t _word(addr_t index) const {
			return index / BITS_PER_WORD; }

		[[nodiscard]] bool _valid_range(addr_t const index,
		                                addr_t const width) const
		{
			size_t num_array_bits = _word_cnt * BITS_PER_WORD;
			bool index_valid = index < num_array_bits;
			bool width_valid = width <= num_array_bits;
			bool end_valid = num_array_bits - width >= index;
			return index_valid && width_valid && end_valid;
		}

		addr_t _mask(addr_t const index, addr_t const width,
		             addr_t &rest) const
		{
			addr_t const shift = index - _word(index) * BITS_PER_WORD;

			rest = width + shift > BITS_PER_WORD ?
			       width + shift - BITS_PER_WORD : 0;

			return (width >= BITS_PER_WORD) ? ~0UL << shift
			                                : ((1UL << width) - 1) << shift;
		}

		[[nodiscard]] bool _set(addr_t index, addr_t width, bool free)
		{
			if (!_valid_range(index, width))
				return false;

			addr_t rest, word, mask;
			do {
				word = _word(index);
				mask = _mask(index, width, rest);

				if (free) {
					if ((_words[word] & mask) != mask)
						return false;
					_words[word] &= ~mask;
				} else {
					if (_words[word] & mask)
						return false;
					_words[word] |= mask;
				}

				index = (_word(index) + 1) * BITS_PER_WORD;
				width = rest;
			} while (rest);
			return true;
		}

	public:

		/**
		 * Return true if at least one bit is set between
		 * index until index + width - 1
		 */
		bool get(addr_t index, addr_t width) const
		{
			if (!_valid_range(index, width))
				return false;

			bool used = false;
			addr_t rest, mask;
			do {
				mask  = _mask(index, width, rest);
				used  = _words[_word(index)] & mask;
				index = (_word(index) + 1) * BITS_PER_WORD;
				width = rest;
			} while (!used && rest);

			return used;
		}

		[[nodiscard]] bool set(addr_t const index, addr_t const width) {
			return _set(index, width, false); }

		[[nodiscard]] bool clear(addr_t const index, addr_t const width) {
			return _set(index, width, true); }

		Bit_array_dynamic(addr_t *addr, unsigned bits)
		: _bit_cnt(bits), _word_cnt(_bit_cnt / BITS_PER_WORD),
		  _words(addr)
		{
			ASSERT(bits && bits % BITS_PER_WORD == 0);
			memset(_words, 0, sizeof(addr_t)*_word_cnt);
		}
};


class Genode::Bit_allocator_dynamic
{
	private:

		/*
		 * Noncopyable
		 */
		Bit_allocator_dynamic(Bit_allocator_dynamic const &);
		Bit_allocator_dynamic &operator = (Bit_allocator_dynamic const &);

		addr_t             _next { 0 };
		Allocator         &_alloc;
		unsigned    const  _bits_aligned;
		addr_t     *const  _ram;
		Bit_array_dynamic  _array;

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE,
		};

		/**
		 * Reserve consecutive number of bits
		 *
		 * \noapi
		 */
		[[nodiscard]] bool _reserve(addr_t bit_start, size_t const num)
		{
			if (!num) return true;

			return _array.set(bit_start, num);
		}

		size_t _ram_size() const
		{
			return (_bits_aligned / BITS_PER_WORD) * sizeof(addr_t);
		}

	public:

		[[nodiscard]] bool alloc_any_range(addr_t &bit_start, size_t const num_log2 = 0)
		{
			addr_t const step = 1UL << num_log2;
			addr_t max = ~0UL;

			do {
				for (addr_t i = _next & ~(step - 1); i < max; i += step) {
					if (_array.get(i, step))
						continue;

					if (!_array.set(i, step))
						break;

					_next = i + step;
					bit_start = i;
					return true;
				}
				max = _next;
				_next = 0;

			} while (max != 0);

			return false;
		}

		[[nodiscard]] bool alloc_given_range(addr_t const bit_start, size_t const num_log2 = 0)
		{
			addr_t const step = 1UL << num_log2;
			if (_array.get(bit_start, step))
				return false;

			if (!_array.set(bit_start, step))
				return false;

			_next = bit_start + step;
			return true;
		}

		[[nodiscard]] bool free(addr_t const bit_start, size_t const num_log2 = 0)
		{
			if (!_array.clear(bit_start, 1UL << num_log2))
				return false;

			_next = bit_start;
			return true;
		}

		Bit_allocator_dynamic(Allocator &alloc, unsigned bits)
		:
			_alloc(alloc),
			_bits_aligned(bits % BITS_PER_WORD ?
			              bits + BITS_PER_WORD - (bits % BITS_PER_WORD) :
			              bits),
			_ram((addr_t *)_alloc.alloc(_ram_size())),
			_array(_ram, _bits_aligned)
		{
			ASSERT(_reserve(bits, _bits_aligned - bits));
		}

		~Bit_allocator_dynamic()
		{
			_alloc.free((void *)_ram, _ram_size());
		}
};

#endif /* _BIT_ALLOCATOR_DYNAMIC_H_ */
