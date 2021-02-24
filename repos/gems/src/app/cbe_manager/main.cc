/*
 * \brief  Graphical front end for controlling CBE devices
 * \author Martin Stein
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
#include <base/attached_rom_dataspace.h>
#include <os/buffered_xml.h>
#include <os/reporter.h>
#include <report_session/report_session.h>

/* local includes */
#include <report_session_component.h>
#include <child_state.h>
#include <sandbox.h>

namespace Cbe_manager {

	class Main;
}


class Cbe_manager::Main : Sandbox::Local_service_base::Wakeup,
                         Sandbox::State_handler
{
	private:

		using Report_service     = Sandbox::Local_service<Report::Session_component>;
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;

		Env                    &_env;
		Heap                    _heap                     { _env.ram(), _env.rm() };
		Attached_rom_dataspace  _config                   { _env, "config" };
		Registry<Child_state>   _children                 { };
		Child_state             _fs_query_child_state     { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Sandbox                 _sandbox                  { _env, *this };
		Report_service          _report_service           { _sandbox, *this };
		bool                    _initial_config           { true };
		Signal_handler<Main>    _config_handler           { _env.ep(), *this, &Main::_handle_config };
		Xml_report_handler      _fs_query_listing_handler { *this, &Main::_handle_fs_query_listing };

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
		}

		void _handle_config()
		{
			_config.update();
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
			_report_service.for_each_requested_session([&] (Report_service::Request &request) {

				if (request.label == "fs_query -> listing") {

					Report::Session_component &session { *new (_heap)
						Report::Session_component(
							_env, _fs_query_listing_handler, _env.ep(),
							request.resources, "", request.diag) };

					request.deliver_session(session);
				}
			});

			_report_service.for_each_session_to_close([&] (Report::Session_component &session) {

				destroy(_heap, &session);
				return Report_service::Close_response::CLOSED;
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
						reconfiguration_needed = true; }); });

			if (reconfiguration_needed)
				_update_sandbox_config();
		}

	public:

		Main(Env &env)
		:
			_env { env }
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
