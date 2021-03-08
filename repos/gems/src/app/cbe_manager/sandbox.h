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
#include <util/string.h>
#include <os/sandbox.h>


void route_to_child_service(Genode::Xml_generator &xml,
                            char const            *child_name,
                            char const            *service_name,
                            char const            *service_label = "")
{
	xml.node("service", [&] () {
		xml.attribute("name", service_name);
		if (Genode::strcmp(service_label, "")) {
			xml.attribute("label", service_label);
		}
		xml.attribute("name", service_name);
		xml.node("child", [&] () {
			xml.attribute("name", child_name);
		});
	});
};


void route_to_parent_service(Genode::Xml_generator &xml,
                             char const            *service_name,
                             char const            *src_label = "",
                             char const            *dst_label = "")
{
	xml.node("service", [&] () {
		xml.attribute("name", service_name);
		if (Genode::strcmp(src_label, "")) {
			xml.attribute("label", src_label);
		}
		xml.node("parent", [&] () {
			if (Genode::strcmp(dst_label, "")) {
				xml.attribute("label", dst_label);
			}
		});
	});
};


void route_to_local_service(Genode::Xml_generator &xml,
                            char const            *service_name,
                            char const            *service_label = "")
{
	xml.node("service", [&] () {
		xml.attribute("name", service_name);
		if (Genode::strcmp(service_label, "")) {
			xml.attribute("label", service_label);
		}
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
