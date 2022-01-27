/*
 * \brief  Wireguard component network backend
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* os includes */
#include <nic_session/connection.h>
#include <uplink_session/connection.h>
#include <nic/packet_allocator.h>
#include <net/ethernet.h>
#include <../../../../os/include/net/udp.h>
#include "../../../../os/include/net/arp.h" /* FIXME: including this normally would clash with a Linux header name */

/* app/wireguard includes */
#include <genode_c_api/wireguard.h>
#include <ipv4_address_prefix.h>

namespace Wireguard
{
	using namespace Net;
	using namespace Genode;

	template <typename CONNECTION> class Net_base;
	class Vpn;
	class Local_net;
}


template <typename CONNECTION>
class Wireguard::Net_base
{
	protected:

		enum { PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE    };
		enum { WG_PROT_PACKET_SIZE = Nic::Packet_allocator::OFFSET_PACKET_SIZE };
		enum { BUF_SIZE    = CONNECTION::Session::QUEUE_SIZE * PACKET_SIZE };

		Heap                & _heap;
		Ipv4_address_prefix   _interface;
		Nic::Packet_allocator _packet_alloc { &_heap };
		bool                  _notify_peers { true };
		CONNECTION            _nic;
		bool                  _local;
		Ipv4_address const    _user_ip;
		Ipv4_address const    _device_ip;

		bool _verbose          { true };
		bool _verbose_pkt_drop { true };

		enum Handle_pkt_result { DROP_PACKET, ACK_PACKET };

		enum Send_pkt_result { SUCCEEDED, FAILED };

		enum {

			ETHERNET_HEADER_SIZE = sizeof(Ethernet_frame),

			ETHERNET_DATA_SIZE_WITH_ARP =
				sizeof(Arp_packet) + ETHERNET_HEADER_SIZE < Ethernet_frame::MIN_SIZE ?
					Ethernet_frame::MIN_SIZE - ETHERNET_HEADER_SIZE :
					sizeof(Arp_packet),

			ETHERNET_CRC_SIZE = sizeof(Genode::uint32_t),

			ARP_PACKET_SIZE =
				ETHERNET_HEADER_SIZE +
				ETHERNET_DATA_SIZE_WITH_ARP +
				ETHERNET_CRC_SIZE,
		};

		bool _link_state() { return true; }

		Handle_pkt_result _drop_pkt(char const *packet_type,
		                            char const *reason)
		{
			if (_verbose_pkt_drop) {
				log("Drop ", packet_type, " - ", reason);
			}
			return Handle_pkt_result::DROP_PACKET;
		}

		template <typename FUNC>
		Send_pkt_result _send(Genode::size_t pkt_size, FUNC && write_to_pkt)
		{
			using Nic_source = ::Nic::Packet_stream_source<::Nic::Session::Policy>;

			if (!_link_state()) {
				if (_verbose) {
					log("Failed sending packet - Link is down");
				}
				return Send_pkt_result::FAILED;
			}
			try {
				Packet_descriptor  pkt        { _nic.tx()->alloc_packet(pkt_size) };
				void              *pkt_base   { _nic.tx()->packet_content(pkt) };
				Size_guard         size_guard { pkt_size };

				write_to_pkt(pkt_base, size_guard);
				_nic.tx()->submit_packet(pkt);
			}
			catch (Nic_source::Packet_alloc_failed) {
				if (_verbose) {
					log("Failed sending packet - Failed allocating packet");
				}
				return Send_pkt_result::FAILED;
			}
			return Send_pkt_result::SUCCEEDED;
		}

		Send_pkt_result _send_arp_reply(Ethernet_frame &request_eth,
		                                Arp_packet     &request_arp)
		{
			return _send(ARP_PACKET_SIZE, [&] (void *reply_base, Size_guard &reply_guard) {

				Ethernet_frame &reply_eth {
					Ethernet_frame::construct_at(reply_base, reply_guard) };

				reply_eth.dst(request_eth.src());
				reply_eth.src(mac_address());
				reply_eth.type(Ethernet_frame::Type::ARP);

				Arp_packet &reply_arp {
					reply_eth.construct_at_data<Arp_packet>(reply_guard) };

				reply_arp.hardware_address_type(Arp_packet::ETHERNET);
				reply_arp.protocol_address_type(Arp_packet::IPV4);
				reply_arp.hardware_address_size(sizeof(Mac_address));
				reply_arp.protocol_address_size(sizeof(Ipv4_address));
				reply_arp.opcode(Arp_packet::REPLY);
				reply_arp.src_mac(mac_address());
				reply_arp.src_ip(request_arp.dst_ip());
				reply_arp.dst_mac(request_arp.src_mac());
				reply_arp.dst_ip(request_arp.src_ip());
			});
		}

		Handle_pkt_result _handle_arp(Ethernet_frame &eth,
		                              Size_guard     &size_guard)
		{
			Arp_packet &arp { eth.data<Arp_packet>(size_guard) };
			if (!arp.ethernet_ipv4()) {
				return _drop_pkt("ARP request", "Targets unknown protocol");
			}
			if (arp.opcode() != Arp_packet::REQUEST) {
				return _drop_pkt("ARP packet", "Is not an ARP request");
			}
			if (!_interface.valid()) {
				return _drop_pkt("ARP request", "I have no IP address so far");
			}
			if (_interface.address != arp.dst_ip()) {
				return _drop_pkt("ARP request", "Doesn't target my IP address");
			}
			if (_verbose) {
				log("Answer ARP request");
			}
			if (_send_arp_reply(eth, arp) != Send_pkt_result::SUCCEEDED) {
				return _drop_pkt("ARP request", "Sending reply failed");
			}
			return Handle_pkt_result::ACK_PACKET;
		}


	public:

		template <typename ... ARGS>
		Net_base(Env & env, Heap & heap, Signal_context_capability sigh,
		         Ipv4_address_prefix iface, bool local,
		         Ipv4_address const user_ip,
		         Ipv4_address const device_ip, ARGS ... args)
		:
			_heap(heap),
			_interface(iface),
			_nic(env, &_packet_alloc, BUF_SIZE, BUF_SIZE, args...),
			_local(local),
			_user_ip(user_ip),
			_device_ip(device_ip)
		{
			_nic.rx_channel()->sigh_ready_to_ack(sigh);
			_nic.rx_channel()->sigh_packet_avail(sigh);
			_nic.tx_channel()->sigh_ack_avail(sigh);
			_nic.tx_channel()->sigh_ready_to_submit(sigh);
		}

		virtual ~Net_base<CONNECTION>() {}

		virtual Net::Mac_address mac_address() = 0;

		void for_each_rx_packet(genode_wg_net_receive_t func)
		{
			typename CONNECTION::Rx::Sink & rx_sink = *_nic.rx();

			for (;;) {

				if (!rx_sink.packet_avail() || !rx_sink.ack_slots_free())
					break;

				typedef Uplink::Packet_descriptor Packet_descriptor;

				Packet_descriptor const packet = rx_sink.peek_packet();

				bool const packet_valid = rx_sink.packet_valid(packet)
				                       && (packet.offset() >= 0);

				if (packet_valid) {

					void *eth_base { rx_sink.packet_content(packet) };
					Size_guard size_guard { packet.size() };
					Ethernet_frame &eth { Ethernet_frame::cast_from(eth_base, size_guard) };
					switch (eth.type()) {
					case Ethernet_frame::Type::ARP:

						log("Received an ARP packet");
						 _handle_arp(eth, size_guard);
						break;

					case Ethernet_frame::Type::IPV4:

						{
							log("Received an IPv4 packet");

							if (_local) {

								/* modify IP header in order to apply user NAT */
								Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
								ip.src(_device_ip);
								ip.update_checksum();
							}

							//FIXME: get listen port and put it into callback
							func(0U, eth_base, packet.size(), _local);
							_notify_peers = true;
							break;
						}

					default:
						_drop_pkt("packet", "Unknown type in Ethernet header");
					}
				}
				(void)rx_sink.try_get_packet();
				rx_sink.try_ack_packet(packet);
			}
		}

		bool tx_one_packet(void * buf, size_t buf_size)
		{
			typename CONNECTION::Tx::Source & tx_source = *_nic.tx();

			/*
			 * Process acknowledgements
			 */

			while (tx_source.ack_avail()) {
				tx_source.release_packet(tx_source.try_get_acked_packet());
				_notify_peers = true;
			}

			/*
			 * Submit packet
			 */

			if (!tx_source.ready_to_submit(1))
				return false;

			typedef Uplink::Packet_descriptor Packet_descriptor;

			Packet_descriptor packet { };
			size_t const max_bytes = Nic::Packet_allocator::OFFSET_PACKET_SIZE;

			try { packet = tx_source.alloc_packet(max_bytes); }
			catch (Uplink::Session::Tx::Source::Packet_alloc_failed) {
				return false; /* packet-stream buffer is saturated */ }

			char * const dst_ptr = tx_source.packet_content(packet);
			size_t const payload_bytes = min(max_bytes, buf_size);
			memcpy(dst_ptr, buf, payload_bytes);

			/* imprint payload size into packet descriptor */
			packet = Packet_descriptor(packet.offset(), payload_bytes);

			tx_source.try_submit_packet(packet);
			_notify_peers = true;

			return true;
		}

		void notify_peer()
		{
			if (_notify_peers) {
				_notify_peers = false;
				_nic.rx()->wakeup();
				_nic.tx()->wakeup();
			}
		}
};


class Wireguard::Vpn : public Net_base<Nic::Connection>
{
	public:

		Vpn(Env & env, Heap & heap, Signal_context_capability sigh,
		    Ipv4_address_prefix iface)
		:
			Net_base<Nic::Connection>(env, heap, sigh, iface, false,
			                          Ipv4_address { }, Ipv4_address { },
			                          "vpn")
		{ }

		Net::Mac_address mac_address() override { return _nic.mac_address(); }

		void send_wg_prot(
			genode_wg_u8_t const *wg_prot_base,
			genode_wg_u64_t       wg_prot_size,
			genode_wg_u16_t       udp_src_port_big_endian,
			genode_wg_u16_t       udp_dst_port_big_endian,
			genode_wg_u32_t       ipv4_src_addr_big_endian,
			genode_wg_u32_t       ipv4_dst_addr_big_endian,
			genode_wg_u8_t        ipv4_dscp_ecn,
			genode_wg_u8_t        ipv4_ttl);
};


class Wireguard::Local_net : public Net_base<Uplink::Connection>
{
	private:

		Net::Mac_address _mac_address() {
			return Net::Mac_address(2U); }

	public:

		Local_net(Env & env, Heap & heap, Signal_context_capability sigh,
		          Ipv4_address_prefix iface,
		          Ipv4_address const user_ip,
		          Ipv4_address const device_ip)
		:
			Net_base<Uplink::Connection>(env, heap, sigh, iface, true,
			                             user_ip, device_ip,
			                             _mac_address(), "local") {}

		Net::Mac_address mac_address() override { return _mac_address(); }

		void send_ip(
			genode_wg_u8_t const *ip_base,
			genode_wg_u64_t       ip_size);
};


void Wireguard::Vpn::send_wg_prot(
	genode_wg_u8_t const *wg_prot_base,
	genode_wg_u64_t       wg_prot_size,
	genode_wg_u16_t       udp_src_port_big_endian,
	genode_wg_u16_t       udp_dst_port_big_endian,
	genode_wg_u32_t,
	genode_wg_u32_t       ipv4_dst_addr_big_endian,
	genode_wg_u8_t        ipv4_dscp_ecn,
	genode_wg_u8_t        ipv4_ttl)
{
	size_t const pkt_size {
		sizeof(Ethernet_frame) + sizeof(Ipv4_packet) + sizeof(Udp_packet) +
		wg_prot_size };

	_send(pkt_size, [&] (void *pkt_base, Size_guard &size_guard) {

		/*
		 * FIXME We should do ARP here instead of assuming a certain MAC
		 */
		Mac_address nic_router_mac { 2 };
		nic_router_mac.addr[5] = 0;

		/* create ETH header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(nic_router_mac);
		eth.src(mac_address());
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header of the reply */
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(ipv4_ttl);
		ip.diff_service_ecn(ipv4_dscp_ecn);
		ip.protocol(Ipv4_packet::Protocol::UDP);
		ip.src_big_endian(_interface.address.to_uint32_big_endian());
		ip.dst_big_endian(ipv4_dst_addr_big_endian);

		/* create UDP header of the reply */
		size_t const udp_off = size_guard.head_size();
		Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
		udp.src_port_big_endian(udp_src_port_big_endian);
		udp.dst_port_big_endian(udp_dst_port_big_endian);

		/* add Wireguard protocol data */
		udp.memcpy_to_data((void *)wg_prot_base, wg_prot_size, size_guard);

		/* fill in header values that need the packet to be complete already */
		udp.length((uint16_t)(size_guard.head_size() - udp_off));
		udp.update_checksum(ip.src(), ip.dst());
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}


void Wireguard::Local_net::send_ip(
	genode_wg_u8_t const *ip_base,
	genode_wg_u64_t       ip_size)
{
	Size_guard ip_guard { ip_size };
	Ipv4_packet const &ip { Ipv4_packet::cast_from(ip_base, ip_guard) };
	if (ip.version() != 4) {
		log("Drop packet - IP versions other than 4 not supported");
		return;
	}
	size_t const pkt_size { sizeof(Ethernet_frame) + ip_size };
	_send(pkt_size, [&] (void *pkt_base, Size_guard &size_guard) {

		/*
		 * FIXME We should do ARP here instead of assuming a certain MAC
		 */
		Mac_address dst_mac { 2 };
		dst_mac.addr[5] = 0;

		/* create ETH header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(dst_mac);
		eth.src(mac_address());
		eth.type(Ethernet_frame::Type::IPV4);

		/* add IP packet as payload */
		eth.memcpy_to_data((void *)ip_base, ip_size, size_guard);

		/* modify IP header in order to apply user NAT */
		Size_guard send_ip_guard { ip_size };
		Ipv4_packet &ip = eth.data<Ipv4_packet>(send_ip_guard);
		ip.dst(_user_ip);
		ip.update_checksum();
	});
	
}
