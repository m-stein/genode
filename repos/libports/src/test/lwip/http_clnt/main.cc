/*
 * \brief  HTTP client test
 * \author Ivan Loskutov
 * \date   2012-12-21
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>
#include <util/string.h>
#include <timer_session/connection.h>
#include <nic/packet_allocator.h>
#include <os/config.h>

extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
#include <netif/etharp.h>
}

#include <lwip/genode.h>


static const char *http_get_request =
"GET / HTTP/1.0\r\nHost: localhost:80\r\n\r\n"; /* simple HTTP request header */

using namespace Genode;

bool static_ip_config(uint32_t & ip, uint32_t & nm, uint32_t & gw)
{
	enum { STR_SZ = 16 };
	char ip_str[STR_SZ] = { 0 };
	char nm_str[STR_SZ] = { 0 };
	char gw_str[STR_SZ] = { 0 };
	Xml_node libc_node = config()->xml_node().sub_node("libc");
	try {
		libc_node.attribute("ip_addr").value(ip_str, STR_SZ);
		libc_node.attribute("netmask").value(nm_str, STR_SZ);
		libc_node.attribute("gateway").value(gw_str, STR_SZ);
	} catch(...) { return false; }
	ip = inet_addr(ip_str);
	nm = inet_addr(nm_str);
	gw = inet_addr(gw_str);
	if (ip == INADDR_NONE || nm == INADDR_NONE || gw == INADDR_NONE) { return false; }
	PINF("static ip config: ip=%s nm=%s gw=%s ", ip_str, nm_str, gw_str);
	return true;
}


/**
 * The client thread simply loops endless,
 * and sends as much 'http get' requests as possible,
 * printing out the response.
 */
int main()
{
	using namespace Genode;

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	static Timer::Connection _timer;
	_timer.msleep(2000);
	lwip_tcpip_init();

	uint32_t ip = 0;
	uint32_t nm = 0;
	uint32_t gw = 0;
	bool static_ip = static_ip_config(ip, nm, gw);

	enum { ADDR_STR_SZ = 16 };
	char serv_addr[ADDR_STR_SZ] = { 0 };
	Xml_node libc_node = config()->xml_node().sub_node("libc");
	try { libc_node.attribute("server_ip").value(serv_addr, ADDR_STR_SZ); }
	catch(...) {
		PERR("Missing \"server_ip\" attribute.");
		throw Xml_node::Nonexistent_attribute();
	}

	if (static_ip) {
		if (lwip_nic_init(ip, nm, gw, BUF_SIZE, BUF_SIZE)) {
			PERR("We got no IP address!");
			return 0;
		}
	} else {
		if( lwip_nic_init(0, 0, 0, BUF_SIZE, BUF_SIZE))
		{
			PERR("We got no IP address!");
			return 0;
		}
	}

	for(int j = 0; j != 5; ++j) {
		_timer.msleep(2000);


		int s = lwip_socket(AF_INET, SOCK_STREAM, 0 );
		if (s < 0) {
			PERR("No socket available!");
			continue;
		}

		unsigned port = 0;
		try { libc_node.attribute("http_port").value(&port); }
		catch (...) {
			PERR("Missing \"http_port\" attribute.");
			throw Xml_node::Nonexistent_attribute();
		}

		struct sockaddr_in addr;
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(serv_addr);

		if((lwip_connect(s, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
			PERR("Could not connect!");
			lwip_close(s);
			continue;
		}

		unsigned long bytes = lwip_send(s, (char*)http_get_request,
		                                Genode::strlen(http_get_request), 0);
		if ( bytes < 0 ) {
			PERR("Couldn't send request ...");
			lwip_close(s);
			continue;
		}

		/* Receive http header and content independently in 2 packets */
		for(int i=0; i<2; i++) {
			char buf[1024];
			ssize_t buflen;
			buflen = lwip_recv(s, buf, 1024, 0);
			if(buflen > 0) {
				 buf[buflen] = 0;
				log("Received \"", String<64>(buf), " ...\"");
			} else
				break;
		}

		/* Close socket */
		lwip_close(s);
	}

	_timer.msleep(2000);
	return 0;
}
