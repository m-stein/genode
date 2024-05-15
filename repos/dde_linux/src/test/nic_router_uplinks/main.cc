/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/component.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

using namespace Genode;

struct Nic_driver { String<16> name; };

struct Main
{
	Env &env;
	Expanding_reporter router_config_reporter { env, "config", "router_config" };
	bool router_config_outdated { true };
	Timer::Connection timer { env };
	Timer::One_shot_timeout<Main> timeout { timer, *this, &Main::handle_timeout };
	unsigned step { 0 };
	Constructible<Nic_driver> nic_driver { };

	void handle_timeout(Duration);

	void update_router_config();

	Main(Env &env);
};


Main::Main(Env &env) : env(env)
{
	log("initialized");
	timeout.schedule(Microseconds(15000ULL*1000));
}

void Main::handle_timeout(Duration)
{
	step++;
	log("step ", step);
	switch (step) {
	case 1:
		nic_driver.construct("nic_drv");
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 2:
		nic_driver.construct("wifi_drv");
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 3:
		nic_driver.destruct();
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 4:
		nic_driver.construct("nic_drv");
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 5:
		nic_driver.construct("nic_drv");
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 6:
		nic_driver.destruct();
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 7:
		nic_driver.construct("wifi_drv");
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 8:
		nic_driver.construct("nic_drv");
		timeout.schedule(Microseconds((5000ULL+step)*1000));
		router_config_outdated = true;
		break;
	case 9:
		nic_driver.destruct();
		router_config_outdated = true;
		break;
	}
	if (router_config_outdated)
		update_router_config();
}


void Main::update_router_config()
{
	log("update router config");
	router_config_reporter.generate([&] (Xml_generator &xml) {
		xml.attribute("dhcp_discover_timeout_sec", "1");
		xml.attribute("verbose_domain_state", "yes");
		xml.attribute("verbose", "yes");
		xml.node("policy", [&] {
			xml.attribute("label_prefix", "ping");
			xml.attribute("domain", "downlink"); });

		if (nic_driver.constructed()) {
			xml.node("policy", [&] {
				xml.attribute("label_prefix", nic_driver->name);
				xml.attribute("domain", "uplink"); });
			xml.node("domain", [&] {
				xml.attribute("name", "uplink");
				xml.node("nat", [&] {
					xml.attribute("domain", "downlink");
					xml.attribute("icmp-ids", "999"); }); });
		}
		xml.node("domain", [&] {
			xml.attribute("name", "downlink");
			xml.attribute("interface", "10.0.1.79/24");
			xml.node("dhcp-server", [&] {
				xml.attribute("ip_first", "10.0.1.80");
				xml.attribute("ip_last", "10.0.1.100"); });

			if (nic_driver.constructed())
				xml.node("icmp", [&] {
					xml.attribute("dst", "0.0.0.0/0");
					xml.attribute("domain", "uplink"); }); }); });

	router_config_outdated = false;
}


void Component::construct(Env &env) { static Main main(env); }
