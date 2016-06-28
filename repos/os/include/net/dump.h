
#include <base/printf.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/tcp.h>

namespace Net
{
	void dump_tcp(void * base, Genode::size_t size)
	{
		Tcp_packet * tcp = new (base) Tcp_packet(size);
		Genode::printf("\033[32mTCP\033[0m f%u%u%u%u%u%u %u > %u ",
			(tcp->flags() >> 0) & 1,
			(tcp->flags() >> 1) & 1,
			(tcp->flags() >> 2) & 1,
			(tcp->flags() >> 3) & 1,
			(tcp->flags() >> 4) & 1,
			(tcp->flags() >> 5) & 1,
			tcp->src_port(),
			tcp->dst_port());
	}


	void dump_dhcp(void * base, Genode::size_t size)
	{
		Dhcp_packet * dhcp = new (base) Dhcp_packet(size);
		Genode::printf("\033[32mDHCP\033[0m c%u %x:%x:%x:%x:%x:%x > %u.%u.%u.%u ",
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


	void dump_udp(void * base, Genode::size_t size)
	{
		Udp_packet * udp = new (base) Udp_packet(size);
		Genode::printf("\033[32mUDP\033[0m %u > %u ",
			udp->src_port(),
			udp->dst_port());

		Genode::size_t data_size = size - sizeof(Udp_packet);
		void * data = udp->data<void>();
		if (Dhcp_packet::is_dhcp(udp)) { dump_dhcp(data, data_size); }
	}


	void dump_ipv4(void * base, Genode::size_t size)
	{
		Ipv4_packet * ipv4 = new (base) Ipv4_packet(size);

		Genode::printf("\033[32mIPV4\033[0m %3u.%3u.%3u.%3u > %3u.%3u.%3u.%3u ",
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
		case Tcp_packet::IP_ID: dump_tcp(data, data_size); break;
		case Udp_packet::IP_ID: dump_udp(data, data_size); break;
		default: ; }
	}

	void dump_arp(void * base, Genode::size_t size)
	{
		Arp_packet * arp = new (base) Arp_packet(size);

		Genode::printf("\033[32mARP\033[0m %3u.%3u.%3u.%3u %2x:%2x:%2x:%2x:%2x:%2x > %3u.%3u.%3u.%3u %2x:%2x:%2x:%2x:%2x:%2x ",
			arp->src_ip().addr[0],
			arp->src_ip().addr[1],
			arp->src_ip().addr[2],
			arp->src_ip().addr[3],
			arp->src_mac().addr[0],
			arp->src_mac().addr[1],
			arp->src_mac().addr[2],
			arp->src_mac().addr[3],
			arp->src_mac().addr[4],
			arp->src_mac().addr[5],
			arp->dst_ip().addr[0],
			arp->dst_ip().addr[1],
			arp->dst_ip().addr[2],
			arp->dst_ip().addr[3],
			arp->dst_mac().addr[0],
			arp->dst_mac().addr[1],
			arp->dst_mac().addr[2],
			arp->dst_mac().addr[3],
			arp->dst_mac().addr[4],
			arp->dst_mac().addr[5]);
	}

	void dump_eth(void * base, Genode::size_t size)
	{
		Ethernet_frame * eth = new (base) Ethernet_frame(size);

		Genode::printf("\033[32mETH\033[0m %2x:%2x:%2x:%2x:%2x:%2x > %2x:%2x:%2x:%2x:%2x:%2x ",
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
		case Ethernet_frame::ARP: dump_arp(data, data_size); break;
		case Ethernet_frame::IPV4: dump_ipv4(data, data_size); break;
		default: ; }
	}
}
