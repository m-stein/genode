/*
 * \brief  Write content of ROM module to LOG
 * \author Norman Feske
 * \date   2015-09-22
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/signal.h>
#include <base/attached_rom_dataspace.h>
#include <util/print_lines.h>
#include <base/component.h>
#include <util/volatile_object.h>
#include <base/debug.h>

namespace Rom_logger { struct Main; }


struct Rom_logger::Main
{
	Genode::Env &_env;

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	Genode::Lazy_volatile_object<Genode::Attached_rom_dataspace> _rom_ds;

	typedef Genode::String<100> Rom_name;

	/**
	 * Name of currently requested ROM module
	 *
	 * Solely used to detect configuration changes.
	 */
	Rom_name _rom_name;

	/**
	 * Signal handler that is invoked when the configuration or the ROM module
	 * changes.
	 */
	void _handle_update();

	Genode::Signal_handler<Main> _update_handler =
		{ _env.ep(), *this, &Main::_handle_update };

	Main(Genode::Env &env) : _env(env)
	{
		_config_rom.sigh(_update_handler);
		_handle_update();
	}
};


template <typename T>
inline Genode::Hex mkhex(T value) {
	return Genode::Hex(value, Genode::Hex::OMIT_PREFIX, Genode::Hex::PAD); }


void Rom_logger::Main::_handle_update()
{
	using namespace Genode;

	_config_rom.update();

	/*
	 * Query name of ROM module from config
	 */
	Rom_name rom_name;
	try {
		_config_rom.xml().attribute("rom").value(&rom_name);
	} catch (...) {
		Genode::warning("could not determine ROM name from config");
		return;
	}

	typedef Genode::String<8> Format_string;
	Format_string format =
		_config_rom.xml().attribute_value("format", Format_string("text"));

	/*
	 * If ROM name changed, reconstruct '_rom_ds'
	 */
	if (rom_name != _rom_name) {
		_rom_ds.construct(rom_name.string());
		_rom_ds->sigh(_update_handler);
		_rom_name = rom_name;
	}

	/*
	 * Update ROM module and print content to LOG
	 */
	if (_rom_ds.constructed()) {
		_rom_ds->update();

		if (_rom_ds->valid()) {
			log("ROM '", _rom_name, "':");

			if (format == "text") {
				Genode::print_lines<200>(_rom_ds->local_addr<char>(), _rom_ds->size(),
				                         [&] (char const *line) { Genode::log("  ", line); });
			} else if (format == "hexdump") {
				uint16_t const *data = _rom_ds->local_addr<uint16_t const>();
				/* dataspaces are always page aligned, therefore multiples of 2*8 bytes */
				Genode::size_t const data_len = _rom_ds->size() / sizeof(uint16_t);
				for (size_t i = 0; i < data_len; i += 8)
					log(mkhex(i)," ",mkhex(data[i+0])," ",mkhex(data[i+1]),
					             " ",mkhex(data[i+2])," ",mkhex(data[i+3]),
					             " ",mkhex(data[i+4])," ",mkhex(data[i+5]),
					             " ",mkhex(data[i+6])," ",mkhex(data[i+7]));
			} else {
				error("unknown format specified by '", _config_rom.xml(),"'");
			}
		} else {
			Genode::log("ROM '", _rom_name, "' is invalid");
		}
	}
}


Genode::size_t Component::stack_size() {
	return 4*1024*sizeof(long); }

void Component::construct(Genode::Env &env) {
	static Rom_logger::Main main(env); }
