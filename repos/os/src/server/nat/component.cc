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

bool Session_component::link_state() { Genode::error("link_state not implemented"); return false; }

void Session_component::link_state_sigh(Signal_context_capability sigh) { Genode::error("link_state_sigh not implemented"); }

Session_component::Session_component
(
	Allocator * allocator, size_t amount, size_t tx_buf_size,
	size_t rx_buf_size, Mac_address vmac, Server::Entrypoint & ep,
	Uplink & uplink, Mac_address nat_mac, Ipv4_address nat_ip,
	Session_label & label, Port_allocator & tcp_port_alloc,
	Port_allocator & udp_port_alloc)
:
	Guarded_range_allocator(allocator, amount),
	Tx_rx_communication_buffers(tx_buf_size, rx_buf_size),
	Session_rpc_object(
		Tx_rx_communication_buffers::tx_ds(),
		Tx_rx_communication_buffers::rx_ds(),
		this->range_allocator(), ep.rpc_ep()),
	Interface(
		ep, uplink.vlan(), nat_mac, nat_ip, guarded_allocator(), label,
		tcp_port_alloc, udp_port_alloc, vmac),
	_mac_node(vmac, this), _uplink(uplink)
{
	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
}


Net::Root::Root
(
	Server::Entrypoint & ep, Uplink & uplink, Allocator * md_alloc,
	Mac_address nat_mac, Port_allocator & tcp_port_alloc,
	Port_allocator & udp_port_alloc)
:
	Root_component<Session_component>(&ep.rpc_ep(), md_alloc),
	_ep(ep), _uplink(uplink), _nat_mac(nat_mac), _tcp_port_alloc(tcp_port_alloc),
	_udp_port_alloc(udp_port_alloc)
{ }


Session_component * Net::Root::_create_session(char const * args)
{
	using namespace Genode;
	memset(nat_ip_addr, 0, MAX_IP_ADDR_LENGTH);
	Session_label label;
	try {
		label = label_from_args(args);
		Session_policy policy(label);
		policy.attribute("src").value(nat_ip_addr, sizeof(nat_ip_addr));

	} catch (Session_policy::No_policy_defined) {

		if (verbose) { log("No matching policy"); }
		throw Root::Unavailable();

	} catch (Xml_node::Nonexistent_attribute) {

		if (verbose) { log("Missing 'src' attribute in policy"); }
		throw Root::Unavailable();
	}

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
		error("Insufficient 'ram_quota', got %zd, need %zd",
		      ram_quota, tx_buf_size + rx_buf_size + session_size);
		throw Root::Quota_exceeded();
	}

	Ipv4_address nat_ip;
	if (nat_ip_addr != 0 && strlen(nat_ip_addr)) {
		nat_ip = Ipv4_packet::ip_from_string(nat_ip_addr);
		if (nat_ip == Ipv4_address()) {
			log("Empty or error NAT IP address. Skipped.");
		}
	}
	Mac_address mac;
	try { mac = _mac_alloc.alloc(); }
	catch (Mac_allocator::Alloc_failed) { return nullptr; };
	return new (md_alloc()) Session_component(
		env()->heap(), ram_quota - session_size, tx_buf_size, rx_buf_size,
		mac, _ep, _uplink, _nat_mac, nat_ip, label, _tcp_port_alloc, _udp_port_alloc);
}
