/*
 * \brief  Test controller for Intel framebuffer driver
 * \author Stefan Kalkowski
 * \date   2016-04-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/printf.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <os/attached_rom_dataspace.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>

using namespace Genode;


static inline void update_connector_config(Xml_generator & xml, Xml_node & node)
{
	xml.node("connector", [&] {
		String<64> name;
		node.attribute("name").value(&name);
		xml.attribute("name", name.string());

		bool connected = node.attribute_value("connected", false);
		xml.attribute("enabled", connected ? "true" : "false");

		unsigned long width = 0, height = 0;
		node.for_each_sub_node("mode", [&] (Xml_node &mode) {
			unsigned long w, h;
			w = mode.attribute_value<unsigned long>("width", 0);
			h = mode.attribute_value<unsigned long>("height", 0);
			if (w > width) {
				width = w;
				height = h;
			}
		});

		if (width && height) {
			xml.attribute("width", width);
			xml.attribute("height", height);
		}
	});
}


void update_fb_config(Xml_node & report)
{
	try {
		static char buf[4096];

		Genode::Xml_generator xml(buf, sizeof(buf), "config", [&] {
			xml.attribute("buffered", "yes");
			xml.node("report", [&] {
				xml.attribute("connectors", "yes");
			});

			report.for_each_sub_node("connector", [&] (Xml_node &node) {
			                         update_connector_config(xml, node); });
		});
		buf[xml.used()] = 0;

		static Allocator_avl fs_packet_alloc { env()->heap() };
		static File_system::Connection fs { fs_packet_alloc, 128*1024, "" };
		File_system::Dir_handle root_dir = fs.dir("/", false);
		File_system::File_handle file =
			fs.file(root_dir, "fb_drv.config", File_system::READ_WRITE, false);
		if (File_system::write(fs, file, buf, xml.used()) == 0)
			PERR("Could not write config");
		fs.close(file);
	} catch (...) {
		PERR("Cannot update config");
	}
}


int main(int argc, char **argv)
{
	Signal_receiver sig_rec;
	Signal_context  sig_ctx;
	Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

	Attached_rom_dataspace rom("connectors");
	rom.sigh(sig_cap);

	for (;;) {
		sig_rec.wait_for_signal();
		rom.update();
		if (!rom.is_valid()) continue;

		Xml_node report(rom.local_addr<char>(), rom.size());
		update_fb_config(report);
	}

	return 0;
}
