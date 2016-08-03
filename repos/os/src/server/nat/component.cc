/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <net/dhcp.h>
#include <net/udp.h>
#include <net/tcp.h>

#include <component.h>

using namespace Net;
using namespace Genode;

static const int verbose = 1;


void Session_component::finalize_packet(Ethernet_frame * eth,
                                        size_t size)
{
	Mac_address_node *node = vlan().mac_tree()->first();
	if (node)
		node = node->find_by_address(eth->dst());
	if (node)
		node->component()->send(eth, size);
	else {
		/* set our MAC as sender */
		eth->src(nat_mac());
		_uplink.send(eth, size);
	}
}


void Session_component::_free_ipv4_node()
{
	if (_ipv4_node) {
		vlan().ip_tree()->remove(_ipv4_node);
		destroy(this->guarded_allocator(), _ipv4_node);
	}
}


bool Session_component::link_state() { return _uplink.link_state(); }


void Session_component::set_port(unsigned port)
{
	_free_port_node();
	_port_node = new (this->guarded_allocator())
		Port_node(port, this);
	vlan().port_tree()->insert(_port_node);
}


void Session_component::_free_port_node()
{
	if (_port_node) {
		vlan().port_tree()->remove(_port_node);
		destroy(this->guarded_allocator(), _port_node);
	}
}


void Session_component::set_ipv4_address(Ipv4_address ip_addr)
{
	_free_ipv4_node();
	_ipv4_node = new (this->guarded_allocator())
		Ipv4_address_node(ip_addr, this);
	vlan().ip_tree()->insert(_ipv4_node);
}


Session_component::Session_component
(
	Allocator * allocator, size_t amount, size_t tx_buf_size,
	size_t rx_buf_size, Mac_address vmac, Server::Entrypoint & ep,
	Uplink & uplink, Ipv4_address ip_addr, Mac_address nat_mac,
	Ipv4_address nat_ip, unsigned port, Session_label & label,
	Port_allocator & port_alloc)
:
	Guarded_range_allocator(allocator, amount),
	Tx_rx_communication_buffers(tx_buf_size, rx_buf_size),
	Session_rpc_object(
		Tx_rx_communication_buffers::tx_ds(),
		Tx_rx_communication_buffers::rx_ds(),
		this->range_allocator(), ep.rpc_ep()),
	Packet_handler(
		ep, uplink.vlan(), nat_mac, nat_ip, guarded_allocator(), label,
		port_alloc, vmac, ip_addr, port),
	_mac_node(vmac, this), _ipv4_node(0), _port_node(0), _uplink(uplink)
{
	vlan().mac_tree()->insert(&_mac_node);
	vlan().mac_list()->insert(&_mac_node);
	if (ip_addr != Ipv4_address()) { set_ipv4_address(ip_addr); }
	if (port) {
		set_port(port);
		port_alloc.alloc_index(port);
	}
	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
}


Session_component::~Session_component() {
	vlan().mac_tree()->remove(&_mac_node);
	vlan().mac_list()->remove(&_mac_node);
	_free_ipv4_node();
	_free_port_node();
}

Net::Root::Root
(
	Server::Entrypoint & ep, Uplink & uplink, Allocator * md_alloc,
	Mac_address nat_mac, Port_allocator & port_alloc)
:
	Root_component<Session_component>(&ep.rpc_ep(), md_alloc),
	_ep(ep), _uplink(uplink), _nat_mac(nat_mac), _port_alloc(port_alloc)
{ }


Session_component * Net::Root::_create_session(char const * args)
{
	using namespace Genode;

	memset(nat_ip_addr, 0, MAX_IP_ADDR_LENGTH);
	memset(ip_addr, 0, MAX_IP_ADDR_LENGTH);
	unsigned port = 0;

	Session_label label;
	 try {
		label = Session_label(args);
		Session_policy policy(label);
		policy.attribute("src").value(nat_ip_addr, sizeof(nat_ip_addr));
		policy.attribute("ip_addr").value(ip_addr, sizeof(ip_addr));
		policy.attribute("port").value(&port);

	} catch (Xml_node::Nonexistent_attribute) {
		if (verbose) PLOG("Missing attribute in policy definition");
	} catch (Session_policy::No_policy_defined) { }

	size_t ram_quota =
		Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
	size_t tx_buf_size =
		Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
	size_t rx_buf_size =
		Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

	/* delete ram quota by the memory needed for the session */
	size_t session_size = max((size_t)4096, sizeof(Session_component));
	if (ram_quota < session_size)
		throw Root::Quota_exceeded();

	/*
	 * Check if donated ram quota suffices for both
	 * communication buffers. Also check both sizes separately
	 * to handle a possible overflow of the sum of both sizes.
	 */
	if (tx_buf_size                  > ram_quota - session_size
		|| rx_buf_size               > ram_quota - session_size
		|| tx_buf_size + rx_buf_size > ram_quota - session_size) {
		PERR("insufficient 'ram_quota', got %zd, need %zd",
		     ram_quota, tx_buf_size + rx_buf_size + session_size);
		throw Root::Quota_exceeded();
	}

	Ipv4_address nat_ip;
	if (nat_ip_addr != 0 && strlen(nat_ip_addr)) {
		nat_ip = Ipv4_packet::ip_from_string(nat_ip_addr);
		if (nat_ip == Ipv4_address() || port == 0) {
			PWRN("Empty or error nat ip address. Skipped.");
		}
	}
	Mac_address mac;
	Ipv4_address ip;
	try { mac = _mac_alloc.alloc(); }
	catch (Mac_allocator::Alloc_failed) { return nullptr; };
	if (ip_addr != 0 && Genode::strlen(ip_addr)) {
		ip = Ipv4_packet::ip_from_string(ip_addr);
	}
	return new (md_alloc()) Session_component(
		env()->heap(), ram_quota - session_size, tx_buf_size, rx_buf_size,
		mac, _ep, _uplink, ip, _nat_mac, nat_ip, port, label, _port_alloc);
}
