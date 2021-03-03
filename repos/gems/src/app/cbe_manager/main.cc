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
#include <dialog.h>
#include <new_file.h>
#include <child_state.h>
#include <sandbox.h>

namespace Cbe_manager {

	class Main;
}

class Cbe_manager::Main
:
	Sandbox::Local_service_base::Wakeup,
	Sandbox::State_handler,
	Gui::Input_event_handler
{
	private:

		using Report_service     = Sandbox::Local_service<Report::Session_component>;
		using Gui_service        = Sandbox::Local_service<Gui::Session_component>;
		using Rom_service        = Sandbox::Local_service<Dynamic_rom_session>;
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;

		Env                                   &_env;
		Heap                                   _heap                      { _env.ram(), _env.rm() };
		Attached_rom_dataspace                 _config                    { _env, "config" };
		Root_directory                         _vfs                       { _env, _heap, _config.xml().sub_node("vfs") };
		Registry<Child_state>                  _children                  { };
		Child_state                            _menu_view_child_state     { _children, "menu_view", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _fs_query_child_state      { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Xml_report_handler                     _fs_query_listing_handler  { *this, &Main::_handle_fs_query_listing };
		Sandbox                                _sandbox                   { _env, *this };
		Gui_service                            _gui_service               { _sandbox, *this };
		Rom_service                            _rom_service               { _sandbox, *this };
		Report_service                         _report_service            { _sandbox, *this };
		Xml_report_handler                     _hover_handler             { *this, &Main::_handle_hover };
		Dialog                                 _dialog                    { _env.ep(), _env.ram(), _env.rm() };
		Constructible<Watch_handler<Main>>     _watch_handler             { };
		Constructible<Expanding_reporter>      _clipboard_reporter        { };
		Constructible<Attached_rom_dataspace>  _clipboard_rom             { };
		bool                                   _initial_config            { true };
		Signal_handler<Main>                   _config_handler            { _env.ep(), *this, &Main::_handle_config };

		void _handle_hover(Xml_node const &node)
		{
			if (!node.has_sub_node("dialog"))
				_dialog.handle_hover(Xml_node("<empty/>"));

			node.with_sub_node("dialog", [&] (Xml_node const &dialog) {
				_dialog.handle_hover(dialog); });
		}

		void _handle_fs_query_listing(Xml_node const &node)
		{
			Genode::log("----- fs_query -> listing -----");
			Genode::log(node);
			Genode::log("-------------------------------");
		}

		void _generate_sandbox_config(Xml_generator &xml) const
		{
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
		}

		Directory::Path _path() const
		{
			return _config.xml().attribute_value("path", Directory::Path());
		}

		void _watch(bool enabled)
		{
			_watch_handler.conditional(enabled, _vfs, _path(), *this, &Main::_handle_watch);
		}

		bool _editable() const
		{
			return !_watch_handler.constructed();
		}

		void _handle_watch()
		{
		}

		void _handle_config()
		{
			_config.update();

			Xml_node const config = _config.xml();

			_watch(config.attribute_value("watch", false));

			_initial_config = false;
		}

		void _update_sandbox_config()
		{
			Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
				_generate_sandbox_config(xml); } };

			config.with_xml_node([&] (Xml_node const &config) {
				_sandbox.apply_config(config); });
		}


		/***************************************************
		 ** Sandbox::Local_service_base::Wakeup interface **
		 ***************************************************/

		void wakeup_local_service() override
		{
			_rom_service.for_each_requested_session([&] (Rom_service::Request &request) {

				if (request.label == "menu_view -> dialog")
					request.deliver_session(_dialog.rom_session());
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


		/****************************
		 ** Sandbox::State_handler **
		 ****************************/

		void handle_sandbox_state() override
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


		/****************************************
		 ** Gui::Input_event_handler interface **
		 ****************************************/

		void handle_input_event(Input::Event const &event) override
		{
			_dialog.handle_input_event(event);
		}

	public:

		Main(Env &env)
		:
			_env(env)
		{
			_config.sigh(_config_handler);
			_handle_config();
			_update_sandbox_config();
		}
};


void Component::construct(Genode::Env &env)
{
	static Cbe_manager::Main main { env };
}
