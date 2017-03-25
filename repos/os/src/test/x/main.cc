/*
 * \brief  Terminal echo program
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-10-16
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <terminal_session/connection.h>

using namespace Genode;

template <size_t BUF_SIZE, typename T>
struct Read_value
{
	struct Bad_input_format : Exception { };

	T                          &dst;
	Signal_transmitter          done;
	Terminal::Connection       &terminal;
	char                        read_buffer[BUF_SIZE];
	unsigned                    read_cnt { 0 };
	Signal_handler<Read_value>  read_avail;

	void is_done(unsigned end)
	{
		read_buffer[end] = '\n';
		read_cnt = 0;
		if (!ascii_to(read_buffer, dst)) {
			throw Bad_input_format(); }
		done.submit();
	}

	void handle_read_avail()
	{
		unsigned num_bytes = terminal.read(&read_buffer[read_cnt],
		                                   BUF_SIZE - read_cnt);

		for (unsigned i = 0; i < num_bytes; i++) {
			if (read_buffer[read_cnt + i] == '\r' || read_buffer[read_cnt + i] == '\n') {
				terminal.write("\n", 1);
				is_done(read_cnt + i);
				return;
			}
			terminal.write(&read_buffer[read_cnt + i], 1);
		}
		read_cnt += num_bytes;
		if (BUF_SIZE <= read_cnt) {
			is_done(BUF_SIZE - 1); }
	}

	Read_value(Entrypoint                &ep,
	           T                         &dst,
	           Terminal::Connection      &terminal,
	           Signal_context_capability  done)
	:
		dst(dst), done(done), terminal(terminal),
		read_avail(ep, *this, &Read_value::handle_read_avail)
	{
		terminal.read_avail_sigh(read_avail);
	}
};

struct Main
{
	Terminal::Connection      terminal;
	Signal_handler<Main>      read_avail;
	unsigned                  val { 0 };
	Read_value<100, unsigned> read_val;

	String<128> intro {
		"--- Terminal echo test started - now you can type characters to be echoed. ---\r\n" };

	void handle_read_avail()
	{
		log("read value: ", val);
	}

	Main(Env &env)
	:
		terminal(env),
		read_avail(env.ep(), *this, &Main::handle_read_avail),
		read_val(env.ep(), val, terminal, read_avail)
	{
		terminal.write(intro.string(), intro.length() + 1);
	}
};

void Component::construct(Env &env) { static Main main(env); }
