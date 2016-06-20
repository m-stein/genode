
#include <base/printf.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/tcp.h>

namespace Net
{
	void dump_tcp(void * base, Genode::size_t size, char const * prefix)
	{
		Tcp_packet * tcp = new (base) Tcp_packet(size);
		PINF("%sTCP src %u dst %u",
			prefix,
			tcp->src_port(),
			tcp->dst_port());
	}


	void dump_dhcp(void * base, Genode::size_t size, char const * prefix)
	{
		Dhcp_packet * dhcp = new (base) Dhcp_packet(size);
		PINF("%sDHCP op %u cln %x:%x:%x:%x:%x:%x srv %u.%u.%u.%u",
			prefix,
			dhcp->op(),
			dhcp->client_mac().addr[0],
			dhcp->client_mac().addr[1],
			dhcp->client_mac().addr[2],
			dhcp->client_mac().addr[3],
			dhcp->client_mac().addr[4],
			dhcp->client_mac().addr[5],
			dhcp->siaddr().addr[0],
			dhcp->siaddr().addr[1],
			dhcp->siaddr().addr[2],
			dhcp->siaddr().addr[3]);
	}


	void dump_udp(void * base, Genode::size_t size, char const * prefix)
	{
		Udp_packet * udp = new (base) Udp_packet(size);
		PINF("%sUDP src %u dst %u",
			prefix,
			udp->src_port(),
			udp->dst_port());

		Genode::size_t data_size = size - sizeof(Udp_packet);
		void * data = udp->data<void>();
		if (Dhcp_packet::is_dhcp(udp)) { dump_dhcp(data, data_size, prefix); }
	}


	void dump_ipv4(void * base, Genode::size_t size, char const * prefix)
	{
		Ipv4_packet * ipv4 = new (base) Ipv4_packet(size);

		PINF("%sIPV4 src %u.%u.%u.%u dst %u.%u.%u.%u",
			prefix,
			ipv4->src().addr[0],
			ipv4->src().addr[1],
			ipv4->src().addr[2],
			ipv4->src().addr[3],
			ipv4->dst().addr[0],
			ipv4->dst().addr[1],
			ipv4->dst().addr[2],
			ipv4->dst().addr[3]);

		void * data = ipv4->data<void>();
		Genode::size_t data_size = size - sizeof(Ipv4_packet);
		switch (ipv4->protocol()) {
		case Tcp_packet::IP_ID: dump_tcp(data, data_size, prefix); break;
		case Udp_packet::IP_ID: dump_udp(data, data_size, prefix); break;
		default: ; }
	}

	void dump_arp(void * base, Genode::size_t size, char const * prefix)
	{
		Arp_packet * arp = new (base) Arp_packet(size);

		PINF("%sARP src %u.%u.%u.%u dst %u.%u.%u.%u",
			prefix,
			arp->src_ip().addr[0],
			arp->src_ip().addr[1],
			arp->src_ip().addr[2],
			arp->src_ip().addr[3],
			arp->dst_ip().addr[0],
			arp->dst_ip().addr[1],
			arp->dst_ip().addr[2],
			arp->dst_ip().addr[3]);
	}

	void dump_eth(void * base, Genode::size_t size, char const * prefix)
	{
		Ethernet_frame * eth = new (base) Ethernet_frame(size);

		PINF("%sETH src %x:%x:%x:%x:%x:%x dst %x:%x:%x:%x:%x:%x",
			prefix,
			eth->src().addr[0],
			eth->src().addr[1],
			eth->src().addr[2],
			eth->src().addr[3],
			eth->src().addr[4],
			eth->src().addr[5],
			eth->dst().addr[0],
			eth->dst().addr[1],
			eth->dst().addr[2],
			eth->dst().addr[3],
			eth->dst().addr[4],
			eth->dst().addr[5]);

		void * data = eth->data<void>();
		Genode::size_t data_size = size - sizeof(Ethernet_frame);
		switch (eth->type()) {
		case Ethernet_frame::ARP: dump_arp(data, data_size, prefix); break;
		case Ethernet_frame::IPV4: dump_ipv4(data, data_size, prefix); break;
		default: ; }
	}
}
