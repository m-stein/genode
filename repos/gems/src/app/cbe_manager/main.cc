/*
 * \brief  Graphical front end for controlling CBE devices
 * \author Martin Stein
 * \author Norman Feske
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/session_object.h>
#include <base/attached_rom_dataspace.h>
#include <base/buffered_output.h>
#include <os/buffered_xml.h>
#include <os/sandbox.h>
#include <os/dynamic_rom_session.h>
#include <os/vfs.h>
#include <os/reporter.h>

/* local includes */
#include <gui_session_component.h>
#include <report_session_component.h>
#include <new_file.h>
#include <child_state.h>
#include <sandbox.h>
#include <passphrase.h>
#include <utf8.h>

namespace Cbe_manager {

	class Main;
}


class Cbe_manager::Main
:
	private Sandbox::Local_service_base::Wakeup,
	private Sandbox::State_handler,
	private Gui::Input_event_handler,
	private Dynamic_rom_session::Xml_producer
{
	private:

		enum { STATE_STRING_CAPACITY = 2 };

		using Report_service     = Sandbox::Local_service<Report::Session_component>;
		using Gui_service        = Sandbox::Local_service<Gui::Session_component>;
		using Rom_service        = Sandbox::Local_service<Dynamic_rom_session>;
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;
		using State_string       = String<STATE_STRING_CAPACITY>;

		enum class State
		{
			INVALID,
			INIT_TRUST_ANCHOR_SETTINGS,
			INIT_TRUST_ANCHOR_IN_PROGRESS,
		};

		enum class Init_ta_settings_hover
		{
			NONE,
			PASSPHRASE_INPUT,
			START_BUTTON
		};

		enum class Init_ta_settings_select
		{
			NONE,
			PASSPHRASE_INPUT,
			START_BUTTON
		};

		Env                                   &_env;
		State                                  _state                     { State::INVALID };
		Heap                                   _heap                      { _env.ram(), _env.rm() };
		Attached_rom_dataspace                 _config                    { _env, "config" };
		Root_directory                         _vfs                       { _env, _heap, _config.xml().sub_node("vfs") };
		Registry<Child_state>                  _children                  { };
		Child_state                            _menu_view_child_state     { _children, "menu_view", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _fs_query_child_state      { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Constructible<Child_state>             _init_ta_child_state       { };
		Xml_report_handler                     _fs_query_listing_handler  { *this, &Main::_handle_fs_query_listing };
		Sandbox                                _sandbox                   { _env, *this };
		Gui_service                            _gui_service               { _sandbox, *this };
		Rom_service                            _rom_service               { _sandbox, *this };
		Report_service                         _report_service            { _sandbox, *this };
		Xml_report_handler                     _hover_handler             { *this, &Main::_handle_hover };
		Constructible<Watch_handler<Main>>     _watch_handler             { };
		Constructible<Expanding_reporter>      _clipboard_reporter        { };
		Constructible<Attached_rom_dataspace>  _clipboard_rom             { };
		bool                                   _initial_config            { true };
		Signal_handler<Main>                   _config_handler            { _env.ep(), *this, &Main::_handle_config };
		Signal_handler<Main>                   _state_handler            { _env.ep(), *this, &Main::_handle_state };
		Dynamic_rom_session                    _dialog                    { _env.ep(), _env.ram(), _env.rm(), *this };
		Passphrase                             _init_ta_setg_passphrase   { };
		Init_ta_settings_hover                 _init_ta_setg_hover        { Init_ta_settings_hover::NONE };
		Init_ta_settings_select                _init_ta_setg_select       { Init_ta_settings_hover::PASSPHRASE_INPUT };

		static State _state_from_string(State_string const &str);

		static State_string _state_to_string(State state);

		static State _state_from_fs_query_listing(Xml_node const &node);

		void _write_to_state_file(State state);

		void _generate_sandbox_config(Xml_generator &xml) const;

		void _handle_fs_query_listing(Xml_node const &node);

		void _handle_hover(Xml_node const &node);

		void _handle_config();
		void _handle_state();

		void _update_sandbox_config();


		/***************************************************
		 ** Sandbox::Local_service_base::Wakeup interface **
		 ***************************************************/

		void wakeup_local_service() override;


		/****************************
		 ** Sandbox::State_handler **
		 ****************************/

		void handle_sandbox_state() override;


		/****************************************
		 ** Gui::Input_event_handler interface **
		 ****************************************/

		void handle_input_event(Input::Event const &event) override;


		/***************************************
		 ** Dynamic_rom_session::Xml_producer **
		 ***************************************/

		void produce_xml(Xml_generator &xml) override;

	public:

		Main(Env &env);
};

using namespace Cbe_manager;


/***********************
 ** Cbe_manager::Main **
 ***********************/

void Main::_handle_config()
{
	_config.update();
	_initial_config = false;
}


void Main::_update_sandbox_config()
{
	Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
		_generate_sandbox_config(xml); } };

	config.with_xml_node([&] (Xml_node const &config) {
		_sandbox.apply_config(config); });
}


Main::State Main::_state_from_string(State_string const &str)
{
	if (str == "1") { return State::INIT_TRUST_ANCHOR_SETTINGS; }
	class Invalid_state_string { };
	throw Invalid_state_string { };
}


Main::State_string Main::_state_to_string(State state)
{
	switch (state) {
	case State::INVALID:                       return "0";
	case State::INIT_TRUST_ANCHOR_SETTINGS:    return "1";
	case State::INIT_TRUST_ANCHOR_IN_PROGRESS: return "2";
	}
	class Invalid_state { };
	throw Invalid_state { };
}


Main::State Main::_state_from_fs_query_listing(Xml_node const &node)
{
	State state { State::INVALID };
	node.with_sub_node("dir", [&] (Xml_node const &node_0) {
		node_0.with_sub_node("file", [&] (Xml_node const &node_1) {
			if (node_1.attribute_value("name", String<6>()) == "state") {
				state = _state_from_string(
					node_1.decoded_content<State_string>());
			}
		});
	});
	return state;
}


void Main::_write_to_state_file(State state)
{
	bool write_error = false;
	try {
		New_file new_file(_vfs, Directory::Path("/cbe/state"));
		auto write = [&] (char const *str)
		{
			switch (new_file.append(str, strlen(str))) {
			case New_file::Append_result::OK:

				break;

			case New_file::Append_result::WRITE_ERROR:

				write_error = true;
				break;
			}
		};
		Buffered_output<STATE_STRING_CAPACITY, decltype(write)> output(write);
		print(output, _state_to_string(state));
	}
	catch (New_file::Create_failed) {

		class Create_state_file_failed { };
		throw Create_state_file_failed { };
	}
	if (write_error) {

		class Write_state_file_failed { };
		throw Write_state_file_failed { };
	}
}


void Main::_handle_fs_query_listing(Xml_node const &node)
{
	if (_state != State::INVALID) {
		return;
	}
	log("--- fs_query ---");
	log(node);
	log("----------------");

	State const state { _state_from_fs_query_listing(node) };
	if (state == State::INVALID) {

		_write_to_state_file(State::INIT_TRUST_ANCHOR_SETTINGS);

	} else {

		_state = state;
		Signal_transmitter(_state_handler).submit();
	}
}


void Main::_handle_state()
{
	_update_sandbox_config();
	_dialog.trigger_update();
}


Main::Main(Env &env)
:
	Xml_producer { "dialog" },
	_env         { env }
{
	_config.sigh(_config_handler);
	_handle_config();
	_update_sandbox_config();
}


void Cbe_manager::Main::handle_sandbox_state()
{
	/* obtain current sandbox state */
	Buffered_xml state(_heap, "state", [&] (Xml_generator &xml) {

		_sandbox.generate_state_report(xml);
	});
	bool reconfiguration_needed = false;
	state.with_xml_node([&] (Xml_node state) {

		state.for_each_sub_node("child", [&] (Xml_node const &child) {

			if (_fs_query_child_state.apply_child_state_report(child))
			{
				reconfiguration_needed = true;
			}
			if (_menu_view_child_state.apply_child_state_report(child))
			{
				reconfiguration_needed = true;
			}
		});
	});
	if (reconfiguration_needed) {
		_update_sandbox_config();
	}
}


void Cbe_manager::Main::produce_xml(Xml_generator &xml)
{
	auto title_min_ex_attr = [] (Xml_generator &xml) {
		xml.attribute("min_ex", "40");
	};
	switch (_state) {
	case State::INVALID:

		xml.node("frame", [&] () {

			xml.node("button", [&] () {
				xml.node("label", [&] () {
					xml.attribute("font", "monospace/regular");
					xml.attribute("text", "Loading...");
					title_min_ex_attr(xml);
				});
			});
		});
		break;

	case State::INIT_TRUST_ANCHOR_SETTINGS:

		xml.node("frame", [&] () {
			xml.node("vbox", [&] () {

				xml.node("button", [&] () {
					xml.attribute("name", "1");
					xml.node("label", [&] () {
						xml.attribute("font", "monospace/regular");
						xml.attribute("text", "Trust anchor initialization");
						title_min_ex_attr(xml);
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
							xml.attribute("name", "pw");
							xml.node("float", [&] () {
								xml.attribute("west", "yes");
								xml.node("label", [&] () {
									xml.attribute("font", "monospace/regular");
									xml.attribute(
										"text",
										String<_init_ta_setg_passphrase.MAX_LENGTH * 3 + 2>(
											" ", _init_ta_setg_passphrase.blind(), " "));

									if (_init_ta_setg_select == Init_ta_settings_select::PASSPHRASE_INPUT) {
										xml.node("cursor", [&] () {
											xml.attribute("at", _init_ta_setg_passphrase.length() + 1);
										});
									}
								});
							});
						});

						if (_init_ta_setg_passphrase.suitable()) {

							xml.node("button", [&] () {
								xml.attribute("name", "ok");
								if (_init_ta_setg_select == Init_ta_settings_select::START_BUTTON) {
									xml.attribute("selected", "yes");
								}
								if (_init_ta_setg_hover == Init_ta_settings_hover::START_BUTTON) {
									xml.attribute("hovered", "yes");
								}
								xml.node("float", [&] () {
									xml.node("label", [&] () {
										xml.attribute("font", "monospace/regular");
										xml.attribute("text", " Start ");
									});
								});
							});

						} else {

							xml.node("float", [&] () {
								xml.attribute("name", "2");
								xml.attribute("west",  "yes");
								xml.node("label", [&] () {
									xml.attribute("font", "monospace/regular");
									xml.attribute("text", _init_ta_setg_passphrase.not_suitable_text());
								});
							});
						}
					});
				});
			});
		});
		break;

	case State::INIT_TRUST_ANCHOR_IN_PROGRESS:

		xml.node("frame", [&] () {
			xml.node("vbox", [&] () {

				xml.node("button", [&] () {
					xml.attribute("name", "1");
					xml.node("label", [&] () {
						xml.attribute("font", "monospace/regular");
						xml.attribute("text", "Trust anchor initialization");
						title_min_ex_attr(xml);
					});
				});

				xml.node("frame", [&] () {
					xml.attribute("name", "2");

					xml.node("float", [&] () {
						xml.attribute("west",  "yes");
						xml.node("label", [&] () {
							xml.attribute("font", "monospace/regular");
							xml.attribute("text", " In progress... ");
						});
					});
				});
			});
		});
		break;
	}
}


void Cbe_manager::Main::wakeup_local_service()
{
	_rom_service.for_each_requested_session([&] (Rom_service::Request &request) {

		if (request.label == "menu_view -> dialog")
			request.deliver_session(_dialog);
		else
			request.deny();
	});

	_report_service.for_each_requested_session([&] (Report_service::Request &request) {

		if (request.label == "fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);
		}
	});

	_report_service.for_each_requested_session([&] (Report_service::Request &request) {

		if (request.label == "menu_view -> hover") {
			Report::Session_component &session = *new (_heap)
				Report::Session_component(_env, _hover_handler,
				                          _env.ep(),
				                          request.resources, "", request.diag);
			request.deliver_session(session);
		}
	});

	_report_service.for_each_session_to_close([&] (Report::Session_component &session) {

		destroy(_heap, &session);
		return Report_service::Close_response::CLOSED;
	});

	_gui_service.for_each_requested_session([&] (Gui_service::Request &request) {

		Gui::Session_component &session = *new (_heap)
			Gui::Session_component(_env, *this, _env.ep(),
			                       request.resources, "", request.diag);

		request.deliver_session(session);
	});

	_gui_service.for_each_upgraded_session([&] (Gui::Session_component &session,
	                                            Session::Resources const &amount) {
		session.upgrade(amount);
		return Gui_service::Upgrade_response::CONFIRMED;
	});

	_gui_service.for_each_session_to_close([&] (Gui::Session_component &session) {

		destroy(_heap, &session);
		return Gui_service::Close_response::CLOSED;
	});
}


void Cbe_manager::Main::_generate_sandbox_config(Xml_generator &xml) const
{
	xml.attribute("verbose",  "yes");
	xml.node("report", [&] () {
		xml.attribute("child_ram",  "yes");
		xml.attribute("child_caps", "yes");
		xml.attribute("delay_ms", 20*1000);
	});

	xml.node("parent-provides", [&] () {
		service_node(xml, "ROM");
		service_node(xml, "CPU");
		service_node(xml, "PD");
		service_node(xml, "LOG");
		service_node(xml, "File_system");
		service_node(xml, "Gui");
		service_node(xml, "Timer");
		service_node(xml, "Report");
	});

	xml.node("start", [&] () {

		_fs_query_child_state.gen_start_node_content(xml);
		xml.node("config", [&] () {
			xml.node("vfs", [&] () {
				xml.node("fs", [&] () {
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("query", [&] () {
				xml.attribute("path", "/");
				xml.attribute("content", "yes");
			});
		});
		xml.node("route", [&] () {
			route_to_local_service(xml, "Report");
			route_to_parent_service(xml, "File_system");
			route_to_parent_service(xml, "PD");
			route_to_parent_service(xml, "ROM");
			route_to_parent_service(xml, "CPU");
			route_to_parent_service(xml, "LOG");
		});
	});

	xml.node("start", [&] () {
		_menu_view_child_state.gen_start_node_content(xml);

		xml.node("config", [&] () {
			xml.attribute("xpos", "100");
			xml.attribute("ypos", "50");

			xml.node("report", [&] () {
				xml.attribute("hover", "yes"); });

			xml.node("libc", [&] () {
				xml.attribute("stderr", "/dev/log"); });

			xml.node("vfs", [&] () {
				xml.node("tar", [&] () {
					xml.attribute("name", "menu_view_styles.tar"); });
				xml.node("dir", [&] () {
					xml.attribute("name", "dev");
					xml.node("log", [&] () { });
				});
				xml.node("dir", [&] () {
					xml.attribute("name", "fonts");
					xml.node("fs", [&] () {
						xml.attribute("label", "fonts");
					});
				});
			});
		});

		xml.node("route", [&] () {
			route_to_local_service(xml, "ROM", "dialog");
			route_to_local_service(xml, "Report", "hover");
			route_to_local_service(xml, "Gui");
			route_to_parent_service(xml, "File_system", "fonts");
			route_to_parent_service(xml, "Timer");
			route_to_parent_service(xml, "PD");
			route_to_parent_service(xml, "ROM");
			route_to_parent_service(xml, "CPU");
			route_to_parent_service(xml, "LOG");
		});
	});

	switch (_state) {
	case State::INVALID:

	case State::INIT_TRUST_ANCHOR_SETTINGS:

		break;

	case State::INIT_TRUST_ANCHOR_IN_PROGRESS:

		xml.node("start", [&] () {

			_init_ta_child_state->gen_start_node_content(xml);
			xml.node("config", [&] () {

				String<_init_ta_setg_passphrase.MAX_LENGTH * 3> const passphrase {
					_init_ta_setg_passphrase };

				xml.attribute("passphrase", passphrase.string());
				xml.attribute("trust_anchor_dir", "/trust_anchor");
				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "trust_anchor");
						xml.node("fs", [&] () {
							xml.attribute("label", "trust_anchor");
						});
					});
				});
			});
			xml.node("route", [&] () {
				route_to_parent_service(xml, "File_system", "trust_anchor");
				route_to_parent_service(xml, "PD");
				route_to_parent_service(xml, "ROM");
				route_to_parent_service(xml, "CPU");
				route_to_parent_service(xml, "LOG");
			});
		});
		break;
	}
}


void Cbe_manager::Main::handle_input_event(Input::Event const &event)
{
	bool update_dialog { false };
	bool update_sandbox_config { false };

	switch (_state) {
	case State::INIT_TRUST_ANCHOR_SETTINGS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Init_ta_settings_select const prev_select { _init_ta_setg_select };
				Init_ta_settings_select       next_select { Init_ta_settings_select::NONE };

				switch (_init_ta_setg_hover) {
				case Init_ta_settings_hover::START_BUTTON:

					next_select = Init_ta_settings_select::START_BUTTON;
					break;

				case Init_ta_settings_hover::PASSPHRASE_INPUT:

					next_select = Init_ta_settings_select::PASSPHRASE_INPUT;
					break;

				case Init_ta_settings_hover::NONE:

					next_select = Init_ta_settings_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_init_ta_setg_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				if (_init_ta_setg_passphrase.suitable() &&
				    _init_ta_setg_select != Init_ta_settings_select::START_BUTTON) {

					_init_ta_setg_select = Init_ta_settings_select::START_BUTTON;
					update_dialog = true;
				}

			} else {

				if (_init_ta_setg_select == Init_ta_settings_select::PASSPHRASE_INPUT) {

					if (codepoint_printable(code)) {

						_init_ta_setg_passphrase.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_init_ta_setg_passphrase.remove_last_character();
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				if (_init_ta_setg_passphrase.suitable() &&
				    _init_ta_setg_select == Init_ta_settings_select::START_BUTTON) {

					_init_ta_setg_select = Init_ta_settings_select::NONE;
					_state = State::INIT_TRUST_ANCHOR_IN_PROGRESS;
					_init_ta_child_state.construct(
						_children,
						"cbe_init_trust_anchor",
						Ram_quota { 4 * 1024 * 1024 },
						Cap_quota { 100 });

					update_sandbox_config = true;
					update_dialog = true;
				}
			}
		});
		break;

	default:

		break;
	}
	if (update_sandbox_config) {
		_update_sandbox_config();
	}
	if (update_dialog) {
		_dialog.trigger_update();
	}
}


void Cbe_manager::Main::_handle_hover(Xml_node const &node)
{
	bool update_dialog { false };

	switch (_state) {
	case State::INIT_TRUST_ANCHOR_SETTINGS:
	{
		Init_ta_settings_hover const prev_hover { _init_ta_setg_hover };
		Init_ta_settings_hover       next_hover { Init_ta_settings_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {

							node_4.with_sub_node("button", [&] (Xml_node const &node_5) {
								if (node_5.attribute_value("name", String<3>()) == "ok") {
									next_hover = Init_ta_settings_hover::START_BUTTON;
								}
							});

							node_4.with_sub_node("frame", [&] (Xml_node const &node_5) {
								if (node_5.attribute_value("name", String<4>()) == "pw") {
									next_hover = Init_ta_settings_hover::PASSPHRASE_INPUT;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_init_ta_setg_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	default:

		break;
	}
	if (update_dialog) {
		_dialog.trigger_update();
	}
}


/***********************
 ** Genode::Component **
 ***********************/

void Component::construct(Genode::Env &env)
{
	static Cbe_manager::Main main { env };
}
