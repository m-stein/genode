/*
 * \brief  Common utilities for parsing and generating ICMPv6 packets
 * \author Martin Stein
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__ICMPV6_H_
#define _NET__ICMPV6_H_

/* os includes */
#include <util/endian.h>

namespace Net {

	using namespace Genode;

	class Icmpv6_packet;
}

class Net::Icmpv6_packet
{
	public:

		using Type = uint8_t;
		using Code = uint8_t;
		using Checksum = uint16_t;

		enum Type_enum : Type
		{
			NDP_ROUTER_SiOLICITATION = 133,
			NDP_ROUTER_ADVERTISEMENT = 134,
			NDP_NEIGHBOR_SOLICITATION = 135,
			NDP_NEIGHBOR_ADVERTISEMENT = 136,
		};

	private:

		enum { SIZE = 4 };

		union {
			uint8_t _u8_array[SIZE];
			uint16_t _u16_array[SIZE / 2];
			uint32_t _u32_array[SIZE / 4];
		};

	public:

		Type type() const { return _u8_array[0]; }

		void type(Type val) { _u8_array[0] = val; }

		Code code() const { return _u8_array[1]; }

		void code(Code val) { _u8_array[1] = val; }

		Checksum checksum() const { return host_to_big_endian(_u16_array[1]); }

		void checksum(Checksum val) { _u16_array[1] = host_to_big_endian(val); }
};

#endif /* _NET__ICMPV6_H_ */
