/*
 * \brief  Allocator using bitmaps
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \date   2012-06-14
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__BIT_ALLOCATOR_H_
#define _INCLUDE__UTIL__BIT_ALLOCATOR_H_

#include <util/bit_array.h>
#include <base/allocator.h>

namespace Genode {

	class Bit_allocator_base;
	class Bit_allocator_dynamic;
	template<unsigned> class Bit_allocator;
}


class Genode::Bit_allocator_base
{
	private:

		addr_t          _next { 0 };
		Bit_array_base &_array;

	protected:

		enum {
			BITS_PER_BYTE = 8UL,
			BITS_PER_WORD = sizeof(addr_t) * BITS_PER_BYTE,
		};

		/**
		 * Reserve consecutive number of bits
		 *
		 * \noapi
		 */
		void _reserve(addr_t bit_start, size_t const num)
		{
			if (!num) return;

			_array.set(bit_start, num);
		}

	public:

		struct Out_of_indices : Exception { };

		Bit_allocator_base(Bit_array_base &array) : _array(array) { }

		addr_t alloc(size_t const num_log2 = 0)
		{
			addr_t const step = 1UL << num_log2;
			addr_t max = ~0UL;

			do {
				try {
					/* throws exception if array is accessed outside bounds */
					for (addr_t i = _next & ~(step - 1); i < max; i += step) {
						if (_array.get(i, step))
							continue;

						_array.set(i, step);
						_next = i + step;
						return i;
					}
				} catch (Bit_array_base::Invalid_index_access) { }

				max = _next;
				_next = 0;

			} while (max != 0);

			throw Out_of_indices();
		}

		void free(addr_t const bit_start, size_t const num_log2 = 0)
		{
			_array.clear(bit_start, 1UL << num_log2);
			_next = bit_start;
		}
};


template<unsigned BITS>
class Genode::Bit_allocator : public Bit_allocator_base
{
	private:

		enum {
			BITS_ALIGNED  = (BITS + BITS_PER_WORD - 1UL)
			                & ~(BITS_PER_WORD - 1UL),
		};

		using Array = Bit_array<BITS_ALIGNED>;

		addr_t _next;
		Array  _array;

	public:

		Bit_allocator() : Bit_allocator_base(_array)
		{
			_reserve(BITS, BITS_ALIGNED - BITS);
		}

		Bit_allocator(Bit_allocator const & o)
		:
			Bit_allocator_base(_array), _array(o._array)
		{ }
};


class Genode::Bit_allocator_dynamic : public Bit_allocator_base
{
	private:

		Allocator         &_alloc;
		unsigned    const  _bits_aligned;
		addr_t     *const  _ram;
		Bit_array_dynamic  _array;

		size_t _ram_size() const
		{
			return (_bits_aligned / BITS_PER_WORD) * sizeof(addr_t);
		}

	public:

		Bit_allocator_dynamic(Allocator &alloc, unsigned bits)
		:
			Bit_allocator_base(_array), _alloc(alloc),
			_bits_aligned(bits % BITS_PER_WORD ?
			              bits + BITS_PER_WORD - (bits % BITS_PER_WORD) :
			              bits),
			_ram((addr_t *)_alloc.alloc(_ram_size())),
			_array(_ram, _bits_aligned)
		{
			_reserve(bits, _bits_aligned - bits);
		}

		~Bit_allocator_dynamic()
		{
			_alloc.free((void *)_ram, _ram_size());
		}
};

#endif /* _INCLUDE__UTIL__BIT_ALLOCATOR_H_ */
