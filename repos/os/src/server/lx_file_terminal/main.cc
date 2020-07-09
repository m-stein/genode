/*
 * \brief  Service providing the 'Terminal_session' interface for a Linux file
 * \author Josef Soentgen
 * \date   2020-07-09
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


namespace Terminal {
	class Session_component;
	class Root_component;
}


class Terminal::Session_component : public Genode::Rpc_object<Session, Session_component>
{
	private:

		Genode::Attached_ram_dataspace    _io_buffer;
		Genode::Signal_context_capability _read_avail_sigh { };

		int       _fd         { 0 };
		ssize_t  _data_avail { 0 };
		size_t   _io_buffer_size;

	public:

		Session_component(Genode::Env &env,
		                  Genode::size_t io_buffer_size, const char *filename)
		:
			_io_buffer(env.ram(), env.rm(), io_buffer_size), _io_buffer_size(io_buffer_size)
		{
			_fd = open(filename, O_RDWR);
			if (_fd < 0) {
				Genode::error("could not open '", filename, "' (errno: ", errno, ")");
				throw Genode::Exception();
			}
		}

		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override { return Size(0, 0); }

		bool avail() override
		{
			return _data_avail > 0;
		}

		Genode::size_t _read(Genode::size_t dst_len)
		{
			if (_data_avail <= 0) return 0;

			size_t count = Genode::min(dst_len, (size_t)_data_avail);

			return count;
		}

		Genode::size_t _write(Genode::size_t num_bytes)
		{
			Genode::log("write: ", num_bytes);
			_data_avail = 0;
			ssize_t written_bytes = ::write(_fd, _io_buffer.local_addr<char>(), num_bytes);

			if (written_bytes < 0) {
				Genode::error("write failed");
				return 0;
			}

			_data_avail = ::read(_fd, _io_buffer.local_addr<char>(), _io_buffer_size);
			if (_data_avail < 0)
				Genode::error("device read failed");

			Genode::log("read: ", _data_avail);

			if (_data_avail > 0 && _read_avail_sigh.valid())
				Genode::Signal_transmitter(_read_avail_sigh).submit();

			return written_bytes;
		}

		Genode::Dataspace_capability _dataspace()
		{
			return _io_buffer.cap();
		}

		void read_avail_sigh(Genode::Signal_context_capability sigh) override
		{
			_read_avail_sigh = sigh;
		}

		void connected_sigh(Genode::Signal_context_capability sigh) override
		{
			Genode::Signal_transmitter(sigh).submit();
		}

		void size_changed_sigh(Genode::Signal_context_capability) override { }

		Genode::size_t read(void *, Genode::size_t) override { return 0; }
		Genode::size_t write(void const *, Genode::size_t) override { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env      &_env;
		Genode::Xml_node  _config;

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Session_label  const label = label_from_args(args);
			Session_policy const policy(label, _config);

			if (!policy.has_attribute("filename")) {
				error("missing \"filename\" attribute in policy definition");
				throw Service_denied();
			}

			typedef String<256> File_name;
			File_name const file_name =
				policy.attribute_value("filename", File_name());
			Genode::log(__func__, " open: ", file_name);

			size_t const io_buffer_size =
				policy.attribute_value("io_buffer_size", 4096UL);

			return new (md_alloc())
			       Session_component(_env, io_buffer_size, file_name.string());
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Genode::Env       &env,
		               Genode::Xml_node   config,
		               Genode::Allocator *md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), md_alloc),
			_env(env), _config(config)
		{ }
};


struct Main
{
	Genode::Env &_env;

	Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
	Genode::Xml_node                _config     { _config_rom.xml() };

	Genode::Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	/* create root interface for service */
	Terminal::Root_component _root { _env, _config, &_sliced_heap };

	Main(Genode::Env &env) : _env(env)
	{
		/* announce service at our parent */
		_env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Main main(env);
}
