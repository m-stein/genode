/*
 * \brief  Text dialog
 * \author Norman Feske
 * \date   2020-01-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <dialog.h>

using namespace Cbe_manager;


enum {
	CODEPOINT_BACKSPACE = 8,      CODEPOINT_NEWLINE  = 10,
	CODEPOINT_UP        = 0xf700, CODEPOINT_DOWN     = 0xf701,
	CODEPOINT_LEFT      = 0xf702, CODEPOINT_RIGHT    = 0xf703,
	CODEPOINT_HOME      = 0xf729, CODEPOINT_INSERT   = 0xf727,
	CODEPOINT_DELETE    = 0xf728, CODEPOINT_END      = 0xf72b,
	CODEPOINT_PAGEUP    = 0xf72c, CODEPOINT_PAGEDOWN = 0xf72d,
};


template <typename T>
static void swap(T &v1, T &v2) { auto tmp = v1; v1 = v2; v2 = tmp; };


void Dialog::produce_xml(Xml_generator &xml)
{
	switch (_window_type) {
	case Window_type::INIT_TRUST_ANCHOR_SETTINGS:
	{
		xml.node("frame", [&] () {
			xml.node("vbox", [&] () {

				xml.node("button", [&] () {
					xml.attribute("name", "1");
					xml.node("label", [&] () {
						xml.attribute("font", "monospace/regular");
						xml.attribute("text", "Trust anchor initialization");
					});
				});

				xml.node("frame", [&] () {
					xml.attribute("name", "2");
					xml.node("vbox", [&] () {

						xml.node("float", [&] () {
							xml.attribute("name", "1");
							xml.attribute("west",  "yes");
							xml.node("label", [&] () {
								xml.attribute("font", "monospace/regular");
								xml.attribute("text", " Enter passphrase: ");
							});
						});

						xml.node("frame", [&] () {
							xml.attribute("name", "pwd");
							xml.node("float", [&] () {
								xml.attribute("west", "yes");
								xml.node("label", [&] () {
									xml.attribute("font", "monospace/regular");
									xml.attribute(
										"text",
										String<_init_ta_setg_passphrase.MAX_LENGTH + 3>(
											" ", *static_cast<Blind_passphrase*>(&_init_ta_setg_passphrase), " "));

									if (_init_ta_setg_select == Init_ta_settings_select::PWD) {
										xml.node("cursor", [&] () {
											xml.attribute("at", _init_ta_setg_passphrase.length() + 1);
										});
									}
								});
							});
						});

						xml.node("button", [&] () {
							xml.attribute("name", "ok");
							if (_init_ta_setg_select == Init_ta_settings_select::OK) {
								log("selected ok");
								xml.attribute("selected", "yes");
							}
							if (_init_ta_setg_hover == Init_ta_settings_hover::OK) {
								log("hovered ok");
								xml.attribute("hovered", "yes");
							}
							xml.node("float", [&] () {
								xml.node("label", [&] () {
									xml.attribute("font", "monospace/regular");
									xml.attribute("text", " Start ");
								});
							});
						});
					});
				});
			});
		});
		break;
	}
	default:

		break;
	}
}


void Dialog::_insert_printable(Codepoint)
{
}


void Dialog::_move_characters(Line &from, Line &to)
{
	/* move all characters of line 'from' to the end of line 'to' */
	Line::Index const first { 0 };
	while (from.exists(first)) {
		from.apply(first, [&] (Codepoint &code) {
			to.append(code); });
		from.destruct(first);
	}
}


void Dialog::handle_input_event(Input::Event const &event)
{
	bool update_dialog { false };

	switch (_window_type) {
	case Window_type::INIT_TRUST_ANCHOR_SETTINGS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Init_ta_settings_select const prev_select { _init_ta_setg_select };
				Init_ta_settings_select       next_select { Init_ta_settings_select::NONE };

				switch (_init_ta_setg_hover) {
				case Init_ta_settings_hover::OK:
					log("select ok");
					next_select = Init_ta_settings_select::OK;
					break;
				case Init_ta_settings_hover::PWD:
					log("select pwd");
					next_select = Init_ta_settings_select::PWD;
					break;
				case Init_ta_settings_hover::NONE:
					log("select none");
					next_select = Init_ta_settings_select::NONE;
					break;
				}

				if (next_select != prev_select) {
					log("select upd");
					_init_ta_setg_select = next_select;
					update_dialog = true;
				}
			} else {

				if (_init_ta_setg_select == Init_ta_settings_select::PWD) {

					if (_printable(code)) {

						_init_ta_setg_passphrase.append_character(code);
						log(String<512>(_init_ta_setg_passphrase));
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_init_ta_setg_passphrase.remove_last_character();
						log(String<512>(_init_ta_setg_passphrase));
						update_dialog = true;
					}
				}
			}
		});
		if (event.key_release(Input::BTN_LEFT)) {

			if (_init_ta_setg_select == Init_ta_settings_select::OK) {

				_init_ta_setg_select = Init_ta_settings_select::NONE;
				update_dialog = true;
			}
		}
		break;

	default:

		break;
	}

	if (update_dialog)
		rom_session.trigger_update();
}


void Dialog::handle_hover(Xml_node const &node_0)
{
	bool rom_session_trigger_update { false };

	switch (_window_type) {
	case Window_type::INIT_TRUST_ANCHOR_SETTINGS:
	{
		Init_ta_settings_hover const prev_hover { _init_ta_setg_hover };
		Init_ta_settings_hover       next_hover { Init_ta_settings_hover::NONE };

		node_0.with_sub_node("frame", [&] (Xml_node node_1) {
			node_1.with_sub_node("vbox", [&] (Xml_node node_2) {
				node_2.with_sub_node("frame", [&] (Xml_node node_3) {
					node_3.with_sub_node("vbox", [&] (Xml_node node_4) {

						node_4.with_sub_node("button", [&] (Xml_node node_5) {
							if (node_5.attribute_value("name", String<3>()) == "ok") {
								log("hover ok");
								next_hover = Init_ta_settings_hover::OK;
							}
						});

						node_4.with_sub_node("frame", [&] (Xml_node node_5) {
							if (node_5.attribute_value("name", String<4>()) == "pwd") {
								log("hover pwd");
								next_hover = Init_ta_settings_hover::PWD;
							}
						});
					});
				});
			});
		});
		if (next_hover == Init_ta_settings_hover::NONE) {
			log("hover none");
		}

		if (next_hover != prev_hover) {

			log("hover upd");
			_init_ta_setg_hover = next_hover;
			rom_session_trigger_update = true;
		}
		break;
	}
	default:

		break;
	}

	if (rom_session_trigger_update) {
		rom_session.trigger_update();
	}
}


Dialog::Dialog(Entrypoint &ep, Ram_allocator &ram, Region_map &rm,
               Allocator &alloc)
:
	Xml_producer("dialog"),
	rom_session(ep, ram, rm, *this),
	_alloc(alloc)
{
	clear();
}


void Dialog::clear()
{
}


void Dialog::insert_at_cursor_position(Codepoint)
{
}


void Dialog::gen_clipboard_content(Xml_generator &) const
{
}
