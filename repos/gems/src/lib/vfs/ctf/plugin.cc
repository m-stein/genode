/*
 * \brief  VFS plugin for creating CTF traces
 * \author Johannes Schlatow
 * \date   2021-07-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <os/vfs.h>
#include <util/xml_generator.h>

#include "value_file_system.h"
#include "xml_file_system.h"
#include "trace_control.h"
#include "ctf_writer.h"

namespace Ctf {
	using namespace Vfs;
	using namespace Genode;

	class Local_factory;
	class File_system;
}

class Ctf::Local_factory : public File_system_factory
{
	private:
		Vfs::Env         &_env;

		Value_file_system<bool, 6>  _enable_fs { "enable", "false\n" };
		Xml_file_system<1024>       _config_fs { "config", "<config/>\n" };

		Trace_control     _trace_control { _env.env(), _env.alloc() };
		Ctf_writer        _ctf_writer    { _env,       _trace_control };

		Watch_handler<Local_factory> _enable_handler {
		  _enable_fs, "/enable", _env.alloc(), *this, &Local_factory::_handle_enable };

		void _handle_enable()
		{
			if (_enable_fs.value()) {
				/* apply config when enabling to get a fresh list of subjects */
				_trace_control.start(_config_fs.xml());
				_ctf_writer.start(_config_fs.xml());
			}
			else {
				_trace_control.stop();
				_ctf_writer.stop();
			}
		}

	public:
		Local_factory(Vfs::Env &env)
		: _env(env)
		{ }

	Vfs::File_system *create(Vfs::Env &, Xml_node node) override
	{
		if (node.has_type(_enable_fs.type()))
			return &_enable_fs;

		if (node.has_type(_config_fs.type()))
			return &_config_fs;

		return nullptr;
	}
};

class Ctf::File_system : private Local_factory,
                         public  Vfs::Dir_file_system
{
	private:

		typedef String<200> Config;

		static Config _config()
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				xml.attribute("name", "ctf");
				xml.node("value", [&] () { xml.attribute("name", "enable"); });
				xml.node("xml",   [&] () { xml.attribute("name", "config"); });
			});
			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env)
		: Local_factory(vfs_env),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config().string()), *this)
		{ }

		char const *type() override { return "ctf"; }
};

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node) override
		{
			return new (env.alloc())
				Ctf::File_system(env);
		}
	};

	static Factory f;
	return &f;
}
