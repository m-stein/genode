/*
 * \brief  Local utilities for the menu view dialog
 * \author Martin Stein
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <menu_view_dialog.h>

using namespace Cbe_manager;


void Cbe_manager::gen_frame_title(Xml_generator &xml,
                                  char    const *title,
                                  char    const *name,
                                  unsigned long  min_width)
{

	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("east", "yes");
		xml.attribute("west", "yes");
		xml.attribute("north", "yes");

		xml.node("button", [&] () {

			xml.node("float", [&] () {
				xml.attribute("name", name);
				xml.attribute("west", "yes");

				xml.node("label", [&] () {
					xml.attribute("font", "monospace/regular");
					xml.attribute("text", title);
					xml.attribute("min_ex", min_width);
				});
			});
		});
	});
}

void Cbe_manager::gen_titled_in_progress_frame(Xml_generator &xml,
                                               char const    *name,
                                               char const    *title,
                                               unsigned long  min_width)
{
	gen_titled_frame(xml, name, title, min_width, [&] (Xml_generator &xml) {

		gen_info_line(xml, "info", "In progress...");
		gen_info_line(xml, "pad", "");
	});
}

void Cbe_manager::gen_action_button_at_bottom(Xml_generator &xml,
                                              char const    *name,
                                              char const    *label,
                                              bool           hovered,
                                              bool           selected)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("east",  "yes");
		xml.attribute("west",  "yes");
		xml.attribute("south",  "yes");

		xml.node("button", [&] () {

			if (hovered) {
				xml.attribute("hovered", "yes");
			}
			if (selected) {
				xml.attribute("selected", "yes");
			}

			xml.node("float", [&] () {

				xml.node("label", [&] () {
					xml.attribute("font", "monospace/regular");
					xml.attribute("text", label);
				});
			});
		});
	});
}

void Cbe_manager::gen_action_button_at_bottom(Xml_generator &xml,
                                              char const    *label,
                                              bool           hovered,
                                              bool           selected)
{
	gen_action_button_at_bottom(xml, label, label, hovered, selected);
}

void Cbe_manager::gen_titled_text_input(Xml_generator     &xml,
                                        char        const *name,
                                        char        const *title,
                                        String<256> const &text,
                                        bool               selected)
{
	xml.node("float", [&] () {
		xml.attribute("name", String<64> { name, "_label" });
		xml.attribute("west", "yes");

		xml.node("label", [&] () {
			xml.attribute("font", "monospace/regular");
			xml.attribute("text", String<64> { " ", title, ": " });
		});
	});
	String<256> const padded_text { " ", text };

	xml.node("frame", [&] () {
		xml.attribute("name", name);
		xml.node("float", [&] () {
			xml.attribute("west", "yes");
			xml.node("label", [&] () {
				xml.attribute("font", "monospace/regular");
				xml.attribute("text", padded_text);

				if (selected) {
					xml.node("cursor", [&] () {
						xml.attribute("at", padded_text.length() - 1 );
					});
				}
			});
		});
	});
}

void Cbe_manager::gen_info_line(Xml_generator     &xml,
                                char        const *name,
                                char        const *text)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("west",  "yes");
		xml.node("label", [&] () {
			xml.attribute("font", "monospace/regular");
			xml.attribute("text", String<256> { " ", text, " "});
		});
	});
}

void Cbe_manager::gen_floating_text_line(Xml_generator &xml,
                                         char    const *name,
                                         char    const *line,
                                         unsigned long  select_at,
                                         unsigned long  select_length)
{
	xml.node("hbox", [&] () {
		xml.attribute("name", name);
		xml.node("float", [&] () {
			xml.attribute("north", "yes");
			xml.attribute("south", "yes");
			xml.attribute("west",  "yes");
			xml.node("label", [&] () {
				xml.attribute("font", "monospace/regular");
				xml.attribute("text", String<256> { " ", line, " "});

				if (select_length > 0) {
					xml.node("selection", [&] () {
						xml.attribute("at",     select_at + 1);
						xml.attribute("length", select_length);
					});
				}
			});
		});
	});
};
