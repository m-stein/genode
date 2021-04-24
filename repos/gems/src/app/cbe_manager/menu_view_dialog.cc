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


void Cbe_manager::gen_normal_font_attribute(Xml_generator &xml)
{
	xml.attribute("font", "text/regular");
}


void Cbe_manager::gen_frame_title(Xml_generator &xml,
                                  char    const *title,
                                  char    const *name,
                                  unsigned long  min_width)
{

	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("west", "yes");
		xml.attribute("north", "yes");

		xml.node("label", [&] () {
			xml.attribute("font", "title/regular");
			xml.attribute("text", String<256> { " ", title } );
			xml.attribute("min_ex", min_width);
		});
	});
	gen_info_line(xml, "pad_0", "");
}

void Cbe_manager::gen_titled_info_frame(Xml_generator &xml,
                                        char const    *name,
                                        char const    *title,
                                        char const    *info,
                                        unsigned long  min_width)
{
	gen_titled_frame(xml, name, title, min_width, [&] (Xml_generator &xml) {

		gen_info_line(xml, "info", info);
		gen_info_line(xml, "pad_1", "");
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
					gen_normal_font_attribute(xml);
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

void Cbe_manager::gen_action_button(Xml_generator &xml,
                                    char const    *name,
                                    char const    *label,
                                    bool           hovered,
                                    bool           selected,
                                    size_t         min_ex)
{
	xml.node("button", [&] () {
		xml.attribute("name", name);

		if (hovered) {
			xml.attribute("hovered", "yes");
		}
		if (selected) {
			xml.attribute("selected", "yes");
		}
		xml.node("label", [&] () {

			if (min_ex != 0) {
				xml.attribute("min_ex", min_ex);
			}
			xml.attribute("text", label);
		});
	});
}

void Cbe_manager::gen_text_input(Xml_generator     &xml,
                                 char        const *name,
                                 String<256> const &text,
                                 bool               selected)
{
	String<256> const padded_text { " ", text };

	xml.node("frame", [&] () {
		xml.attribute("name", name);
		xml.node("float", [&] () {
			xml.attribute("west", "yes");
			xml.node("label", [&] () {
				gen_normal_font_attribute(xml);
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

void Cbe_manager::gen_input_passphrase(Xml_generator          &xml,
                                       Input_passphrase const &passphrase,
                                       bool                    input_selected,
                                       bool                    show_hide_button_hovered,
                                       bool                    show_hide_button_selected)
{
	char const *show_hide_button_label;
	size_t cursor_at;
	if (passphrase.hide()) {

		show_hide_button_label = "Show";
		cursor_at = passphrase.length() + 1;

	} else {

		show_hide_button_label = "Hide";
		cursor_at = passphrase.length() + 1;
	}
	xml.node("float", [&] () {
		xml.attribute("name", "Passphrase Label");
		xml.attribute("west", "yes");

		xml.node("label", [&] () {
			gen_normal_font_attribute(xml);
			xml.attribute("text", " Passphrase: ");
		});
	});
	xml.node("hbox", [&] () {

		String<256> const padded_text { " ", passphrase, " " };
		xml.node("frame", [&] () {
			xml.attribute("name", "Passphrase");
			xml.node("float", [&] () {
				xml.attribute("west", "yes");
				xml.node("label", [&] () {
					xml.attribute("min_ex", "41");
					gen_normal_font_attribute(xml);
					xml.attribute("text", padded_text);


					if (input_selected) {
						xml.node("cursor", [&] () {
							xml.attribute("at", cursor_at );
						});
					}
				});
			});
		});
		xml.node("float", [&] () {
			xml.attribute("name", "1");
			xml.attribute("east", "yes");

			gen_action_button(
				xml, "Show Hide", show_hide_button_label, show_hide_button_hovered,
				show_hide_button_selected, 5);
		});
	});
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
			gen_normal_font_attribute(xml);
			xml.attribute("text", String<64> { " ", title, ": " });
		});
	});
	gen_text_input(xml, name, text, selected);
}

void Cbe_manager::gen_info_line(Xml_generator     &xml,
                                char        const *name,
                                char        const *text)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("west",  "yes");
		xml.node("label", [&] () {
			gen_normal_font_attribute(xml);
			xml.attribute("text", String<256> { " ", text, " "});
		});
	});
}

void Cbe_manager::gen_multiple_choice_entry(Xml_generator &xml,
                                           char     const *name,
                                           char     const *text,
                                           bool            hovered,
                                           bool            selected)
{
	xml.node("float", [&] () {
		xml.attribute("name", name);
		xml.attribute("west", "yes");

		xml.node("hbox", [&] () {

			xml.node("button", [&] () {
				if (selected) {
					xml.attribute("selected", "yes");
				}
				if (hovered) {
					xml.attribute("hovered", "yes");
				}
				xml.attribute("style", "radio");

				xml.node("hbox", [&] () { });
			});
			xml.node("label", [&] () {
				gen_normal_font_attribute(xml);
				xml.attribute("text", String<64> { " ", text });
			});
		});
	});
}

void Cbe_manager::gen_sub_menu_title(Xml_generator &xml,
                                     char    const *text,
                                     bool           hovered,
                                     bool           selected)
{
	xml.node("float", [&] () {
		xml.attribute("name", "expand");
		xml.attribute("west", "yes");

		xml.node("hbox", [&] () {

			xml.node("button", [&] () {
				if (selected) {
					xml.attribute("style", "back");
					xml.attribute("selected", "yes");
				} else {
					xml.attribute("style", "radio");
				}
				if (hovered) {
					xml.attribute("hovered", "yes");
				}
				xml.attribute("hovered", "no");

				xml.node("hbox", [&] () { });
			});
			xml.node("label", [&] () {
				xml.attribute("font", "title/regular");
				xml.attribute("text", String<64> { " ", text });
			});
		});
	});
}


void Cbe_manager::gen_closed_sub_menu(Xml_generator &xml,
                                      char    const *name,
                                      bool           hovered)
{
	xml.node("vbox", [&] () {
		xml.attribute("name", name);

		gen_sub_menu_title(xml, name, hovered, false);
	});
}
