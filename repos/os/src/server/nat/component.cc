/*
 * \brief  Downlink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <component.h>
#include <arp_cache.h>
#include <port_allocator.h>

using namespace Net;
using namespace Genode;

static const int verbose = 1;


bool Session_component::link_state()
{
	warning(__func__, " not implemented");
	return false;
}


void Session_component::link_state_sigh(Signal_context_capability sigh)
{
	warning(__func__, " not implemented");
}


Net::Session_component::Session_component(Allocator           &allocator,
                                          size_t const         amount,
                                          size_t const         tx_buf_size,
                                          size_t const         rx_buf_size,
                                          Mac_address          mac,
                                          Server::Entrypoint  &ep,
                                          Mac_address          nat_mac,
                                          Ipv4_address         nat_ip,
                                          Session_label       &label,
                                          Port_allocator      &tcp_port_alloc,
                                          Port_allocator      &udp_port_alloc,
                                          Tcp_proxy_role_list &tcp_proxy_roles,
                                          Udp_proxy_role_list &udp_proxy_roles,
                                          unsigned const       rtt_sec,
                                          Interface_tree      &interface_tree,
                                          Arp_cache           &arp_cache,
                                          Arp_waiter_list     &arp_waiters)
:
	Guarded_range_allocator(allocator, amount),
	Tx_rx_communication_buffers(tx_buf_size, rx_buf_size),

	Session_rpc_object(
		Tx_rx_communication_buffers::tx_ds(),
		Tx_rx_communication_buffers::rx_ds(),
		&range_allocator(), ep.rpc_ep()),

	Interface(
		ep, nat_mac, nat_ip, guarded_allocator(), label, tcp_port_alloc,
		udp_port_alloc, mac, tcp_proxy_roles, udp_proxy_roles, rtt_sec,
		interface_tree, arp_cache, arp_waiters)
{
	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
}


Session_component::~Session_component() { }


Net::Root::Root(Server::Entrypoint  &ep,
                Allocator           &md_alloc,
                Mac_address          nat_mac,
                Port_allocator      &tcp_port_alloc,
                Port_allocator      &udp_port_alloc,
                Tcp_proxy_role_list &tcp_proxy_roles,
                Udp_proxy_role_list &udp_proxy_roles,
                unsigned             rtt_sec,
                Interface_tree      &interface_tree,
                Arp_cache           &arp_cache,
                Arp_waiter_list     &arp_waiters)
:
	Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
	_ep(ep), _nat_mac(nat_mac), _tcp_port_alloc(tcp_port_alloc),
	_udp_port_alloc(udp_port_alloc), _tcp_proxy_roles(tcp_proxy_roles),
	_udp_proxy_roles(udp_proxy_roles), _rtt_sec(rtt_sec),
	_interface_tree(interface_tree), _arp_cache(arp_cache),
	_arp_waiters(arp_waiters)
{ }


Session_component *Net::Root::_create_session(char const *args)
{
	Session_label label;
	Ipv4_address src;
	try {
		label = label_from_args(args);
		Session_policy policy(label);
		src = policy.attribute_value("src", Ipv4_address());

	} catch (Session_policy::No_policy_defined) {

		error("No matching policy");
		throw Root::Unavailable();
	}
	if (src == Ipv4_address()) {

		error("Missing 'src' attribute in policy");
		throw Root::Unavailable();
	}
	size_t const ram_quota =
		Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);

	size_t const tx_buf_size =
		Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

	size_t const rx_buf_size =
		Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

	size_t const session_size = max((size_t)4096, sizeof(Session_component));
	if (ram_quota < session_size) {
		throw Root::Quota_exceeded(); }

	if (tx_buf_size               > ram_quota - session_size ||
	    rx_buf_size               > ram_quota - session_size ||
	    tx_buf_size + rx_buf_size > ram_quota - session_size)
	{
		error("insufficient 'ram_quota' for session creation");
		throw Root::Quota_exceeded();
	}
	Mac_address mac;
	try { mac = _mac_alloc.alloc(); }
	catch (Mac_allocator::Alloc_failed) {

		error("failed to allocate MAC address");
		throw Root::Unavailable();
	}
	return new (md_alloc())
		Session_component(*env()->heap(), ram_quota - session_size,
		                  tx_buf_size, rx_buf_size, mac, _ep, _nat_mac,
		                  src, label, _tcp_port_alloc, _udp_port_alloc,
		                  _tcp_proxy_roles, _udp_proxy_roles, _rtt_sec,
		                  _interface_tree, _arp_cache, _arp_waiters);
}


Tx_rx_communication_buffers::
Tx_rx_communication_buffers(Genode::size_t const tx_size,
                            Genode::size_t const rx_size)
:
	_tx_buf(tx_size), _rx_buf(rx_size)
{ }


Communication_buffer::Communication_buffer(Genode::size_t size)
:
	Ram_dataspace_capability(Genode::env()->ram_session()->alloc(size))
{ }


Range_allocator &Guarded_range_allocator::range_allocator()
{
	return *static_cast<Range_allocator *>(&_range_alloc);
}


Guarded_range_allocator::
Guarded_range_allocator(Allocator &backing_store, size_t const amount)
:
	_guarded_alloc(&backing_store, amount), _range_alloc(&_guarded_alloc)
{ }
