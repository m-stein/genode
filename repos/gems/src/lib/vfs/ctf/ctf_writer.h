/*
 * \brief  CTF file writer
 * \author Johannes Schlatow
 * \date   2021-08-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF_WRITER_H_
#define _CTF_WRITER_H_

#include <os/vfs.h>
#include <os/path.h> /* path_from_label() */
#include <timer_session/connection.h>
#include <rtc_session/connection.h>
#include <ctf/packet_types.h>

#include "packet_buffer.h"

namespace Ctf {
	using Genode::Signal_handler;
	using Genode::Directory;
	using Genode::New_file;
	using Ctf::Packet_header;

	class Ctf_writer;
}

class Ctf::Ctf_writer
{
	private:

		Vfs::Env                  &_env;
		Trace_control             &_trace_control;
		Packet_buffer<32*1024>     _packet_buf      { };
		Directory                  _root_dir        { _env };
		Directory::Path            _cur_path        { };
		Rtc::Connection            _rtc             { _env.env() };
		Timer::Connection          _timer           { _env.env() };
		Signal_handler<Ctf_writer> _timeout_handler {
		  _env.env().ep(), *this, &Ctf_writer::_handle_timeout  };

		void _write_to_file(Genode::New_file &dst, Directory::Path const &path)
		{
			if (_packet_buf.empty())
				return;

			if (dst.append(_packet_buf.data(), _packet_buf.length()) != New_file::Append_result::OK)
				error("Write error for ", path);
		}

		void _handle_timeout()
		{
			using namespace Genode;
			typedef Path<Session_label::capacity()> Label_path;

			_trace_control.for_each_buffer([&] (Trace_buffer &buf, ::Subject_info const &info) {

				try {
					Label_path label_path = path_from_label<Label_path>(info.session_label().string());
					Directory::Path  dst_path(Directory::join(Directory::join(_cur_path,
					                                                          label_path.string()),
					                                          info.thread_name()));
					New_file         dst_file(_root_dir, dst_path, true);

					/* initialise packet header */
					_packet_buf.init_header(info);

					/* copy trace buffer entries to packet buffer */
					buf.for_each_new_entry([&] (Trace::Buffer::Entry &entry) {
						if (entry.length() == 0)
							return true;

						/* write to file if buffer is full */
						if (_packet_buf.bytes_remaining() < entry.length()) {
							_write_to_file(dst_file, dst_path);
							_packet_buf.reset();
						}

						_packet_buf.add_event(*(Event_header_base*)entry.data(), entry.length());
						return true;
					});

					/* write buffer to file */
					_write_to_file(dst_file, dst_path);
				}
				catch (New_file::Create_failed) { }
			});
		}

	public:

		Ctf_writer(Vfs::Env &env, Trace_control &control)
		: _env(env),
		  _trace_control(control)
		{
			_timer.sigh(_timeout_handler);
		}

		void start(Xml_node config)
		{
			unsigned period_ms { 0 };
			if (!config.has_attribute("period_ms"))
				Genode::error("missing XML attribute 'period_ms'");
			else
				period_ms = config.attribute_value("period_ms", period_ms);

			/* read target_root from config */
			Directory::Path target_root = config.attribute_value("target_root", Directory::Path("/"));

			/* set output path based on current time */
			_cur_path = Directory::join(target_root, _rtc.current_time());

			/* start periodic timer */
			_timer.trigger_periodic(period_ms * 1000);
		}

		void stop()
		{
			_timer.trigger_periodic(0);
		}
};


#endif /* _CTF_WRITER_H_ */
