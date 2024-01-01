/*
 * \brief  Type-safe, fine-grained access to a continuous MMIO region
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__MMIO_H_
#define _INCLUDE__UTIL__MMIO_H_

/* Genode includes */
#include <util/string.h>
#include <util/register_set.h>

namespace Genode {

	class Mmio_plain_access;
	template <size_t> class Mmio;
}

/**
 * Plain access implementation for MMIO
 */
class Genode::Mmio_plain_access
{
	friend Register_set_plain_access;

	private:

		Byte_range_ptr const _range;

		/**
		 * Write '_ACCESS_T' typed 'value' to MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline void _write(off_t const offset, ACCESS_T const value)
		{
			if (offset + sizeof(ACCESS_T) > _range.num_bytes) {
				class Bad_offset { };
				throw Bad_offset { };
			}
			addr_t const dst = (addr_t)_range.start + offset;
			*(ACCESS_T volatile *)dst = value;
		}

		/**
		 * Read '_ACCESS_T' typed from MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline ACCESS_T _read(off_t const &offset) const
		{
			if (offset + sizeof(ACCESS_T) > _range.num_bytes) {
				class Bad_offset { };
				throw Bad_offset { };
			}
			addr_t const dst = (addr_t)_range.start + offset;
			ACCESS_T const value = *(ACCESS_T volatile *)dst;
			return value;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param base  base address of targeted MMIO region
		 */
		Mmio_plain_access(Byte_range_ptr const &range) : _range(range.start, range.num_bytes) { }

		addr_t base() const { return (addr_t)_range.start; }
};


/**
 * Type-safe, fine-grained access to a continuous MMIO region
 *
 * For further details refer to the documentation of the 'Register_set' class.
 */
template <Genode::size_t SIZE>
struct Genode::Mmio : Mmio_plain_access, Register_set<Mmio_plain_access, SIZE>
{
	/**
	 * Constructor
	 *
	 * \param base  base address of targeted MMIO region
	 */
	Mmio(Byte_range_ptr const &range)
	:
		Mmio_plain_access(range),
		Register_set<Mmio_plain_access, SIZE>(*static_cast<Mmio_plain_access *>(this))
	{
		if (range.num_bytes > SIZE) {
			class Bad_size { };
			throw Bad_size { };
		}
	}
};

#endif /* _INCLUDE__UTIL__MMIO_H_ */
