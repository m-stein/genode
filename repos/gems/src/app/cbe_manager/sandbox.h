/*
 * \brief  Local utilities for the sandbox API
 * \author Martin Stein
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SANDBOX_H_
#define _SANDBOX_H_

/* Genode includes */
#include <os/sandbox.h>

void route_to_child_service(Genode::Xml_generator &xml,
                            char const            *service_name,
                            char const            *child_name)
{
	xml.node("service", [&] () {
		xml.attribute("name", service_name);
		xml.node("child", [&] () {
			xml.attribute("name", child_name);
		});
	});
};


void route_to_parent_service(Genode::Xml_generator &xml,
                             char const            *service_name)
{
	xml.node("service", [&] () {
		xml.attribute("name", service_name);
		xml.node("parent", [&] () { });
	});
};


void route_to_local_service(Genode::Xml_generator &xml,
                            char const            *service_name)
{
	xml.node("service", [&] () {
		xml.attribute("name", service_name);
		xml.node("local", [&] () { });
	});
};

void service_node(Genode::Xml_generator &xml,
                  char const            *service_name)
{
	xml.node("service", [&] () {
		xml.attribute("name", service_name);
	});
};

#endif /*  SANDBOX_H_ */
