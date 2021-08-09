/*
 * \brief  Packet header and context types
 * \author Johannes Schlatow
 * \date   2021-08-04
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__PACKET_TYPES_H_
#define _CTF__PACKET_TYPES_H_

#include <base/trace/types.h>
#include <ctf/timestamp.h>
#include <util/register.h>

namespace Ctf {
	using namespace Genode;
	using Genode::Trace::Thread_name;
	struct Packet_header;
}

struct Ctf::Packet_header
{
	uint32_t       _magic                 { 0xC1FC1FC1 };
	uint32_t       _stream_id             { };
	Timestamp_base _timestamp_start       { };
	Timestamp_base _timestamp_end         { };
	uint32_t       _total_length          { };
	uint16_t       _hdr_length            { sizeof(Packet_header) * 8 };
	uint16_t       _affinity              { };
	uint8_t        _priority              { };
	char           _session_and_thread[0] { };

	struct Affinity : Register<16>
	{
		struct Xpos   : Bitfield<0,4>  { };
		struct Ypos   : Bitfield<4,4>  { };
		struct Width  : Bitfield<8,4>  { };
		struct Height : Bitfield<12,4> { };
	};

	Packet_header(Session_label              const &label,
	              Thread_name                const &thread,
	              Genode::Affinity::Location const &affinity,
	              unsigned                          priority,
	              Genode::size_t                    buflen,
	              unsigned                          streamid=0)
	: _stream_id(streamid),
	  _affinity(Affinity::Xpos::bits(affinity.xpos())   |
	            Affinity::Ypos::bits(affinity.ypos())   |
	            Affinity::Width::bits(affinity.width()) |
	            Affinity::Height::bits(affinity.height())),
	  _priority(priority)
	{
		Genode::size_t sess_len = Genode::min(label.length() + 1, buflen);
		Genode::copy_cstring(_session_and_thread, label.string(), sess_len);
		_hdr_length += (sess_len-1) * 8;

		if (sess_len < buflen) {
			Genode::size_t thread_len = Genode::min(thread.length() + 1, buflen - sess_len);
			Genode::copy_cstring(&_session_and_thread[sess_len-1], thread.string(), thread_len);
			_hdr_length += (thread_len-1) * 8;
		}

		_total_length = _hdr_length;
	}

	void reset()
	{
		_total_length    = _hdr_length;
		_timestamp_start = 0;
		_timestamp_end   = 0;
	}

	/**
 	 * Update struct with timestamp and length of new event.
 	 *
 	 * Makes sure that timestamps are monotonically increasing and therefore
 	 * returns the added timestamp.
 	 */
	Timestamp_base append_event(Timestamp_base timestamp, uint32_t length)
	{
		using Ctf::Timestamp;

		if (_timestamp_start == 0)
			_timestamp_start = timestamp;
		else if (Timestamp::extended() &&
		         Timestamp::Base::get(_timestamp_end) > Timestamp::Base::get(timestamp)) {
			/* timer wrapped, increase manually managed offset */
			Timestamp::Extension::set(_timestamp_end, Timestamp::Extension::get(_timestamp_end)+1);
		}

		Timestamp::Base::set(_timestamp_end, timestamp);
		_total_length += length * 8;

		return _timestamp_end;
	}

	uint32_t total_length_bytes() const { return _total_length / 8; }
	bool     empty()              const { return _total_length <= _hdr_length; }

	void *operator new(__SIZE_TYPE__, void *p) { return p; }
} __attribute((packed));

#endif /* _CTF__PACKET_TYPES_H_ */
