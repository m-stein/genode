/*
 * \brief  Minimal HTTP server lwIP demonstration
 * \author lwIP Team
 * \author Stefan Kalkowski
 * \date   2009-10-23
 *
 * This small example shows how to use the LwIP in Genode directly.
 * If you simply want to use LwIP's socket API, you might use
 * Genode's libc together with its LwIP backend, especially useful
 * when porting legacy code.
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>
#include <util/string.h>
#include <nic/packet_allocator.h>
#include <os/config.h>
#include <base/snprintf.h>

/* LwIP includes */
extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}

#include <lwip/genode.h>


const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"; /* HTTP response header */

enum { HTTP_INDEX_HTML_SZ = 1024 };

static char http_index_html[HTTP_INDEX_HTML_SZ]; /* HTML page */


/**
 * Handle a single client's request.
 *
 * \param conn  socket connected to the client
 */
void http_server_serve(int conn) {
	char    buf[1024];
	ssize_t buflen;

	/* Read the data from the port, blocking if nothing yet there.
	   We assume the request (the part we care about) is in one packet */
	buflen = lwip_recv(conn, buf, 1024, 0);
	PLOG("Packet received!");

	/* Ignore all receive errors */
	if (buflen > 0) {

		/* Is this an HTTP GET command? (only check the first 5 chars, since
		   there are other formats for GET, and we're keeping it very simple)*/
		if (buflen >= 5 &&
			buf[0] == 'G' &&
			buf[1] == 'E' &&
			buf[2] == 'T' &&
			buf[3] == ' ' &&
			buf[4] == '/' ) {

			PLOG("Will send response");

			/* Send http header */
			lwip_send(conn, http_html_hdr, Genode::strlen(http_html_hdr), 0);

			/* Send our HTML page */
			lwip_send(conn, http_index_html, Genode::strlen(http_index_html), 0);
		}
	}
}


int main()
{
	using namespace Genode;

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	int s;

	lwip_tcpip_init();

	enum { ADDR_STR_SZ = 16 };

	char ip_addr_str[ADDR_STR_SZ] = { 0 };
	char netmask_str[ADDR_STR_SZ] = { 0 };
	char gateway_str[ADDR_STR_SZ] = { 0 };

	uint32_t ip = 0;
	uint32_t nm = 0;
	uint32_t gw = 0;
	unsigned port = 0;

	Xml_node libc_node = config()->xml_node().sub_node("libc");

	try { libc_node.attribute("ip_addr").value(ip_addr_str, ADDR_STR_SZ); }
	catch(...) {
		PERR("Missing \"ip_addr\" attribute.");
		throw Xml_node::Nonexistent_attribute();
	}
	try { libc_node.attribute("netmask").value(netmask_str, ADDR_STR_SZ); }
	catch(...) {
		PERR("Missing \"netmask\" attribute.");
		throw Xml_node::Nonexistent_attribute();
	}
	try { libc_node.attribute("gateway").value(gateway_str, ADDR_STR_SZ); }
	catch(...) {
		PERR("Missing \"gateway\" attribute.");
		throw Xml_node::Nonexistent_attribute();
	}
	try { libc_node.attribute("http_port").value(&port); }
	catch(...) {
		PERR("Missing \"http_port\" attribute.");
		throw Xml_node::Nonexistent_attribute();
	}

	PDBG("static network interface: ip_addr=%s netmask=%s gateway=%s ",
		ip_addr_str, netmask_str, gateway_str);

	ip = inet_addr(ip_addr_str);
	nm = inet_addr(netmask_str);
	gw = inet_addr(gateway_str);

	if (ip == INADDR_NONE || nm == INADDR_NONE || gw == INADDR_NONE) {
		PERR("Invalid network interface config.");
		throw -1;
	}

	/* Initialize network stack  */
	if (lwip_nic_init(ip, nm, gw, BUF_SIZE, BUF_SIZE)) {
		PERR("We got no IP address!");
		return -1;
	}

	PLOG("Create new socket ...");
	if((s = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		PERR("No socket available!");
		return -1;
	}

	Genode::snprintf(
		http_index_html, HTTP_INDEX_HTML_SZ,
		"<html><head><title>Congrats!</title></head><body><h1>Welcome to our lwIP HTTP server at port %u!</h1><p>This is a small test page.</body></html>",
		port);

	PLOG("Now, I will bind ...");
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	if(lwip_bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		PERR("bind failed!");
		return -1;
	}

	PLOG("Now, I will listen ...");
	if(lwip_listen(s, 5)) {
		PERR("listen failed!");
		return -1;
	}

	PLOG("Start the server loop ...");
	while(true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = lwip_accept(s, &addr, &len);
		if(client < 0) {
			PWRN("Invalid socket from accept!");
			continue;
		}
		http_server_serve(client);
		lwip_close(client);
	}
	return 0;
}
