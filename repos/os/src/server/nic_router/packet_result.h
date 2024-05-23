/*
 * \brief  Result of handling a packet
 * \author Martin Stein
 * \date   2024-05-23
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PACKET_RESULT_H_
#define _PACKET_RESULT_H_

namespace Net {

	struct Packet_result
	{
		enum Type {
			INVALID, DROP, POSTPONED, HANDLED, OUT_OF_QUOTA, ICMP_LINK_OUT_OF_QUOTA,
			UDP_LINK_OUT_OF_QUOTA, TCP_LINK_OUT_OF_QUOTA } type { INVALID };

		char const *message { "" };

		bool valid() const { return type != INVALID; }
	};

	inline Packet_result packet_drop(char const *message) { return { Packet_result::DROP, message }; }

	inline Packet_result packet_out_of_quota(char const *message) { return { Packet_result::OUT_OF_QUOTA, message }; }

	inline Packet_result packet_postponed() { return { Packet_result::POSTPONED, "" }; }

	inline Packet_result packet_handled() { return { Packet_result::HANDLED, "" }; }
}

#endif /* _PACKET_RESULT_H_ */
