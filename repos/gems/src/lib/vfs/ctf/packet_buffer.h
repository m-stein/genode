/*
 * \brief  Convenience helper for creating a CTF packet
 * \author Johannes Schlatow
 * \date   2021-08-06
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PACKET_BUFFER_H_
#define _PACKET_BUFFER_H_

#include <ctf/packet_types.h>
#include <ctf/event_header.h>

#include "subject_info.h"

template <unsigned BUFSIZE>
class Packet_buffer
{
	public:
		struct Buffer_too_small : Genode::Exception { };

	private:
		char _buffer[BUFSIZE] { };

	Ctf::Packet_header &_header() { return *(Ctf::Packet_header*)_buffer; }

	public:

		void init_header(Subject_info const &info)
		{
			new (_buffer) Ctf::Packet_header(info.session_label(),
			                                 info.thread_name(),
			                                 info.affinity(),
			                                 info.priority(),
			                                 BUFSIZE);
		}

		void add_event(Ctf::Event_header_base &event, Genode::size_t length)
		{
			if (bytes_remaining() < length)
				throw Buffer_too_small();

			unsigned offset = _header().total_length_bytes();

			/* append timestamp to header and update (potentially) corrected timestamp in event */
			event.timestamp(_header().append_event(event.timestamp(), length));

			Genode::memcpy(&_buffer[offset], &event, length);
		}

		void           reset()           { _header().reset(); }
		bool           empty()           { return _header().empty(); }
		Genode::size_t length()          { return _header().total_length_bytes(); }
		const char    *data()            { return _buffer; }
		Genode::size_t bytes_remaining() { return BUFSIZE - _header().total_length_bytes(); }
};


#endif /* _PACKET_BUFFER_H_ */
