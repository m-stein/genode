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
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/udp.h>

#include <component.h>

using namespace Net;

static const int verbose = 1;


bool Session_component::handle_arp(Ethernet_frame *eth, Genode::size_t size)
{
	Arp_packet *arp =
		new (eth->data<void>()) Arp_packet(size - sizeof(Ethernet_frame));
	if (arp->ethernet_ipv4() &&
		arp->opcode() == Arp_packet::REQUEST) {

		/*
		 * 'Gratuitous ARP' broadcast messages are used to announce newly created
		 * IP<->MAC address mappings to other hosts. nic_bridge-internal hosts
		 * would expect a nic_bridge-internal MAC address in this message, whereas
		 * external hosts would expect the NIC's MAC address in this message.
		 * The simplest solution to this problem is to just drop those messages,
		 * since they are not really necessary.
		 */
		 if (arp->src_ip() == arp->dst_ip())
			return false;

		Ipv4_address_node *node = vlan().ip_tree()->first();
		if (node)
			node = node->find_by_address(arp->dst_ip());
		if (!node) {
			arp->src_mac(_nic.mac());
		}
	}
	return true;
}


bool Session_component::handle_ip(Ethernet_frame *eth, Genode::size_t size)
{
	Ipv4_packet *ip =
		new (eth->data<void>()) Ipv4_packet(size - sizeof(Ethernet_frame));

	if (ip->protocol() == Udp_packet::IP_ID)
	{
		Udp_packet *udp = new (ip->data<void>())
			Udp_packet(size - sizeof(Ipv4_packet));
		if (Dhcp_packet::is_dhcp(udp)) {
			Dhcp_packet *dhcp = new (udp->data<void>())
				Dhcp_packet(size - sizeof(Ipv4_packet) - sizeof(Udp_packet));
			if (dhcp->op() == Dhcp_packet::REQUEST) {
				dhcp->broadcast(true);
				udp->update_checksum(ip->src(), ip->dst());
			}
		}
	}
	return true;
}


void Session_component::finalize_packet(Ethernet_frame *eth,
                                                    Genode::size_t size)
{
	Mac_address_node *node = vlan().mac_tree()->first();
	if (node)
		node = node->find_by_address(eth->dst());
	if (node)
		node->component()->send(eth, size);
	else {
		/* set our MAC as sender */
		eth->src(_nic.mac());
		_nic.send(eth, size);
	}
}


void Session_component::_free_ipv4_node()
{
	if (_ipv4_node) {
		vlan().ip_tree()->remove(_ipv4_node);
		destroy(this->guarded_allocator(), _ipv4_node);
	}
}


bool Session_component::link_state() { return _nic.link_state(); }


void Session_component::set_ipv4_address(Ipv4_address ip_addr)
{
	_free_ipv4_node();
	_ipv4_node = new (this->guarded_allocator())
		Ipv4_address_node(ip_addr, this);
	vlan().ip_tree()->insert(_ipv4_node);
}


Session_component::Session_component(Genode::Allocator          *allocator,
                                     Genode::size_t              amount,
                                     Genode::size_t              tx_buf_size,
                                     Genode::size_t              rx_buf_size,
                                     Mac_address vmac,
                                     Server::Entrypoint         &ep,
                                     Net::Nic                   &nic,
                                     Ipv4_address ip)
: Guarded_range_allocator(allocator, amount),
  Tx_rx_communication_buffers(tx_buf_size, rx_buf_size),
  Session_rpc_object(Tx_rx_communication_buffers::tx_ds(),
                     Tx_rx_communication_buffers::rx_ds(),
                     this->range_allocator(), ep.rpc_ep()),
  Packet_handler(ep, nic.vlan()),
  _mac_node(vmac, this),
  _ipv4_node(0),
  _nic(nic)
{
	vlan().mac_tree()->insert(&_mac_node);
	vlan().mac_list()->insert(&_mac_node);

	if (ip != Ipv4_address()) { set_ipv4_address(ip); }

	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
}


Session_component::~Session_component() {
	vlan().mac_tree()->remove(&_mac_node);
	vlan().mac_list()->remove(&_mac_node);
	_free_ipv4_node();
}
