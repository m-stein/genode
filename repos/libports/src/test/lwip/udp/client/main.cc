/*
 * \brief  Simple UDP test client
 * \author Martin Stein
 * \date   2017-10-18
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <test/lwip/socket.h>

/* Genode includes */
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>
#include <util/string.h>

/* libc includes */
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace Genode;
using Ipv4_addr_str = Genode::String<16>;


static void test(Libc::Env &env)
{
	Timer::Connection timer(env);

	/* try to send and receive a message multiple times */
	while (1)
	{
		usleep(1);

		/* create socket */
		Socket socket { SOCK_DGRAM };
		if (!socket.descriptor_valid()) {
			continue;
		}
		/* read server IP address and port */
		Attached_rom_dataspace config(env, "config");
		Xml_node config_node = config.xml();

		auto check_attr = [&] (char const *attr)
		{
			if (config_node.has_attribute(attr))
				return true;

			error("cannot read attribute '", attr, "'");
			return false;
		};

		if (!check_attr("server_ip") || !check_attr("server_port"))
			break;

		Ipv4_addr_str const server_addr =
			config_node.attribute_value("server_ip", Ipv4_addr_str());

		unsigned const port =
			config_node.attribute_value("server_port", 0U);

		/* create server socket address */
		struct sockaddr_in addr;
		socklen_t addr_sz = sizeof(addr);
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(server_addr.string());

		/* send test message */
		enum { BUF_SZ = 1024 };
		String<BUF_SZ> const message("UDP server at ", server_addr, ":", port);
		if (sendto(socket.descriptor(), message.string(), BUF_SZ, 0, (struct sockaddr*)&addr, addr_sz) != BUF_SZ) {
			continue;
		}

		/* receive and print what has been received */
		char buf[BUF_SZ] { };
		if (recvfrom(socket.descriptor(), buf, BUF_SZ, 0, (struct sockaddr*)&addr, &addr_sz) != BUF_SZ) {
			continue;
		}
		log("Received \"", String<64>(buf), " ...\"");
	}
}

void Libc::Component::construct(Libc::Env &env) { with_libc([&] () { test(env); }); }
