/*
 * \brief  Common utilities for parsing and generating IPv6 packets
 * \author Martin Stein
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__IPV6_H_
#define _NET__IPV6_H_

/* os includes */
#include <util/endian.h>

namespace Net {

	using namespace Genode;

	class Ipv6_address;
	class Ipv6_packet;
}

class Net::Ipv6_address
{
	private:

		enum { SIZE = 16 };

		uint8_t _u8_array[SIZE] { 0 };

	public:

		Ipv6_address() { }
};

class Net::Ipv6_packet
{
	public:

		using Version = uint8_t;
		using Traffic_class = uint8_t;
		using Flow_label = uint32_t;
		using Payload_length = uint16_t;
		using Next_header = uint8_t;
		using Hop_limit = uint8_t;

	private:

		enum { SIZE = 40 };
		enum { SRC_ADDR_OFF = 2 * 32 };
		enum { DST_ADDR_OFF = 6 * 32 };

		struct Offset_0_u32 : Genode::Register<32>
		{
			struct Version : Bitfield<0, 4> { };
			struct Traffic_class : Bitfield<4, 8> { };
			struct Flow_label : Bitfield<12, 20> { };
		};

		union {
			uint8_t _u8_array[SIZE];
			uint16_t _u16_array[SIZE / 2];
			uint32_t _u32_array[SIZE / 4];
		};

	public:

		Ipv6_address const &dst_addr() const { return *(Ipv6_address *)&_u8_array[DST_ADDR_OFF]; }

		void dst_addr(Ipv6_address const &addr) { memcpy(&_u8_array[DST_ADDR_OFF], &addr, sizeof(addr)); }

		Ipv6_address const &src_addr() const { return *(Ipv6_address *)&_u8_array[SRC_ADDR_OFF]; }

		void src_addr(Ipv6_address const &addr) { memcpy(&_u8_array[SRC_ADDR_OFF], &addr, sizeof(addr)); }

		Version version() const { return Offset_0_u32::Version::get(_u32_array[0]); }

		Traffic_class traffic_class() const { return Offset_0_u32::Traffic_class::get(_u32_array[0]); }

		Payload_length payload_length() const { return host_to_big_endian(_u16_array[2]); }

		void payload_length(Payload_length val) { _u16_array[2] = host_to_big_endian(val); }

		Next_header next_header() const { return _u8_array[6]; }

		void next_header(Next_header val) { _u8_array[6] = val; }

		Hop_limit hop_limit() const { return _u8_array[7]; }

		void hop_limit(Hop_limit val) { _u8_array[7] = val; }
};

#endif /* _NET__IPV6_H_ */
