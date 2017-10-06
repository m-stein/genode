/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/tcp.h>
#include <net/udp.h>
#include <net/arp.h>

/* local includes */
#include <interface.h>
#include <configuration.h>
#include <l3_protocol.h>

using namespace Net;
using namespace Genode;

/**
 * Utility to ensure that a size value doesn't exceed a limit
 */
template <size_t MAX, typename EXCEPTION>
class Size_guard
{
	private:

		size_t _curr { 0 };

	public:

		void add(size_t size)
		{
			size_t const new_size = _curr + size;
			if (new_size > MAX) { throw EXCEPTION(); }
			_curr = new_size;
		}

		size_t curr() const { return _curr; }
};


/***************
 ** Utilities **
 ***************/

template <typename LINK_TYPE>
static void _destroy_closed_links(Link_list   &closed_links,
                                  Deallocator &dealloc)
{
	while (Link *link = closed_links.first()) {
		closed_links.remove(link);
		destroy(dealloc, static_cast<LINK_TYPE *>(link));
	}
}


template <typename LINK_TYPE>
static void _destroy_links(Link_side_tree &links,
                           Link_list      &closed_links,
                           Deallocator    &dealloc)
{
	_destroy_closed_links<LINK_TYPE>(closed_links, dealloc);
	while (Link_side *link_side = links.first()) {
		Link &link = link_side->link();
		link.dissolve();
		destroy(dealloc, static_cast<LINK_TYPE *>(&link));
	}
}


static void _link_packet(L3_protocol  const  prot,
                         void        *const  prot_base,
                         Link               &link,
                         bool         const  client)
{
	switch (prot) {
	case L3_protocol::TCP:
		if (client) {
			static_cast<Tcp_link *>(&link)->client_packet(*(Tcp_packet *)(prot_base));
			return;
		} else {
			static_cast<Tcp_link *>(&link)->server_packet(*(Tcp_packet *)(prot_base));
			return;
		}
	case L3_protocol::UDP:
		static_cast<Udp_link *>(&link)->packet();
		return;
	default: throw Interface::Bad_transport_protocol(); }
}


static void _update_checksum(L3_protocol   const prot,
                             void         *const prot_base,
                             size_t        const prot_size,
                             Ipv4_address  const src,
                             Ipv4_address  const dst)
{
	switch (prot) {
	case L3_protocol::TCP:
		((Tcp_packet *)prot_base)->update_checksum(src, dst, prot_size);
		return;
	case L3_protocol::UDP:
		((Udp_packet *)prot_base)->update_checksum(src, dst);
		return;
	default: throw Interface::Bad_transport_protocol(); }
}


static Port _dst_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP: return (*(Tcp_packet *)prot_base).dst_port();
	case L3_protocol::UDP: return (*(Udp_packet *)prot_base).dst_port();
	default: throw Interface::Bad_transport_protocol(); }
}


static void _dst_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP: (*(Tcp_packet *)prot_base).dst_port(port); return;
	case L3_protocol::UDP: (*(Udp_packet *)prot_base).dst_port(port); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static Port _src_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP: return (*(Tcp_packet *)prot_base).src_port();
	case L3_protocol::UDP: return (*(Udp_packet *)prot_base).src_port();
	default: throw Interface::Bad_transport_protocol(); }
}


static void _src_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP: ((Tcp_packet *)prot_base)->src_port(port); return;
	case L3_protocol::UDP: ((Udp_packet *)prot_base)->src_port(port); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static void *_prot_base(L3_protocol const  prot,
                        size_t      const  prot_size,
                        Ipv4_packet       &ip)
{
	switch (prot) {
	case L3_protocol::TCP: return new (ip.data<void>()) Tcp_packet(prot_size);
	case L3_protocol::UDP: return new (ip.data<void>()) Udp_packet(prot_size);
	default: throw Interface::Bad_transport_protocol(); }
}


/***************
 ** Interface **
 ***************/

void Interface::_pass_prot(Ethernet_frame       &eth,
                           size_t         const  eth_size,
                           Ipv4_packet          &ip,
                           L3_protocol    const  prot,
                           void          *const  prot_base,
                           size_t         const  prot_size)
{
	_update_checksum(prot, prot_base, prot_size, ip.src(), ip.dst());
	_pass_ip(eth, eth_size, ip);
}


void Interface::_pass_ip(Ethernet_frame &eth,
                         size_t   const  eth_size,
                         Ipv4_packet    &ip)
{
	ip.checksum(Ipv4_packet::calculate_checksum(ip));
	_send(eth, eth_size);
}


Forward_rule_tree &
Interface::_forward_rules(L3_protocol const prot) const
{
	switch (prot) {
	case L3_protocol::TCP: return _domain.tcp_forward_rules();
	case L3_protocol::UDP: return _domain.udp_forward_rules();
	default: throw Bad_transport_protocol(); }
}


Transport_rule_list &
Interface::_transport_rules(L3_protocol const prot) const
{
	switch (prot) {
	case L3_protocol::TCP: return _domain.tcp_rules();
	case L3_protocol::UDP: return _domain.udp_rules();
	default: throw Bad_transport_protocol(); }
}


void
Interface::_new_link(L3_protocol                   const  protocol,
                     Link_side_id                  const &local,
                     Pointer<Port_allocator_guard> const  remote_port_alloc,
                     Interface                           &remote_interface,
                     Link_side_id                  const &remote)
{
	switch (protocol) {
	case L3_protocol::TCP:
		{
			Tcp_link &link = *new (_alloc)
				Tcp_link(*this, local, remote_port_alloc, remote_interface,
				         remote, _timer, _config(), protocol);
			_tcp_links.insert(&link.client());
			remote_interface._tcp_links.insert(&link.server());
			if (_config().verbose()) {
				log("New TCP client link: ", link.client(), " at ", *this);
				log("New TCP server link: ", link.server(),
				    " at ", remote_interface._domain);
			}
			return;
		}
	case L3_protocol::UDP:
		{
			Udp_link &link = *new (_alloc)
				Udp_link(*this, local, remote_port_alloc, remote_interface,
				         remote, _timer, _config(), protocol);
			_udp_links.insert(&link.client());
			remote_interface._udp_links.insert(&link.server());
			if (_config().verbose()) {
				log("New UDP client link: ", link.client(), " at ", *this);
				log("New UDP server link: ", link.server(),
				    " at ", remote_interface._domain);
			}
			return;
		}
	default: throw Bad_transport_protocol(); }
}


Link_side_tree &Interface::_links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP: return _tcp_links;
	case L3_protocol::UDP: return _udp_links;
	default: throw Bad_transport_protocol(); }
}


void Interface::link_closed(Link &link, L3_protocol const prot)
{
	_closed_links(prot).insert(&link);
}


void Interface::ip_allocation_expired(Ip_allocation &allocation)
{
	_release_ip_allocation(allocation);
	_released_ip_allocations.insert(&allocation);
}


void Interface::dissolve_link(Link_side &link_side, L3_protocol const prot)
{
	_links(prot).remove(&link_side);
}


Link_list &Interface::_closed_links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP: return _closed_tcp_links;
	case L3_protocol::UDP: return _closed_udp_links;
	default: throw Bad_transport_protocol(); }
}


void Interface::_adapt_eth(Ethernet_frame          &eth,
                           size_t            const  eth_size,
                           Ipv4_address      const &ip,
                           Packet_descriptor const &pkt,
                           Interface               &interface)
{
	Ipv4_address const &hop_ip = interface._domain.next_hop(ip);
	try { eth.dst(interface._arp_cache.find_by_ip(hop_ip).mac()); }
	catch (Arp_cache::No_match) {
		interface._broadcast_arp_request(hop_ip);
		new (_alloc) Arp_waiter(*this, interface, hop_ip, pkt);
		throw Packet_postponed();
	}
	eth.src(_router_mac);
}


void Interface::_nat_link_and_pass(Ethernet_frame        &eth,
                                   size_t          const  eth_size,
                                   Ipv4_packet           &ip,
                                   L3_protocol     const  prot,
                                   void           *const  prot_base,
                                   size_t          const  prot_size,
                                   Link_side_id    const &local,
                                   Interface             &interface)
{
	Pointer<Port_allocator_guard> remote_port_alloc;
	try {
		Nat_rule &nat = interface._domain.nat_rules().find_by_domain(_domain);
		if(_config().verbose()) {
			log("Using NAT rule: ", nat); }

		_src_port(prot, prot_base, nat.port_alloc(prot).alloc());
		ip.src(interface._router_ip());
		remote_port_alloc.set(nat.port_alloc(prot));
	}
	catch (Nat_rule_tree::No_match) { }
	Link_side_id const remote = { ip.dst(), _dst_port(prot, prot_base),
	                              ip.src(), _src_port(prot, prot_base) };
	_new_link(prot, local, remote_port_alloc, interface, remote);
	interface._pass_prot(eth, eth_size, ip, prot, prot_base, prot_size);
}


void Interface::_send_dhcp_reply(Dhcp_server               const &dhcp_srv,
                                 Mac_address               const &client_mac,
                                 Ipv4_address              const &client_ip,
                                 Dhcp_packet::Message_type        msg_type,
                                 uint32_t                         xid)
{
	/* allocate buffer for the reply */
	enum { BUF_SIZE = 512 };
	void *buf;
	try { _alloc.alloc(BUF_SIZE, &buf); }
	catch (...) { throw Alloc_dhcp_reply_buffer_failed(); }

	/* create ETH header of the reply */
	Size_guard<BUF_SIZE, Dhcp_reply_buffer_too_small> reply_size;
	reply_size.add(sizeof(Ethernet_frame));
	Ethernet_frame &reply_eth = *reinterpret_cast<Ethernet_frame *>(buf);
	reply_eth.dst(client_mac);
	reply_eth.src(_router_mac);
	reply_eth.type(Ethernet_frame::Type::IPV4);

	/* create IP header of the reply */
	enum { IPV4_TIME_TO_LIVE = 64 };
	size_t const reply_ip_off = reply_size.curr();
	reply_size.add(sizeof(Ipv4_packet));
	Ipv4_packet &reply_ip = *reply_eth.data<Ipv4_packet>();
	reply_ip.header_length(sizeof(Ipv4_packet) / 4);
	reply_ip.version(4);
	reply_ip.diff_service(0);
	reply_ip.identification(0);
	reply_ip.flags(0);
	reply_ip.fragment_offset(0);
	reply_ip.time_to_live(IPV4_TIME_TO_LIVE);
	reply_ip.protocol(Ipv4_packet::Protocol::UDP);
	reply_ip.src(_router_ip());
	reply_ip.dst(client_ip);

	/* create UDP header of the reply */
	size_t const reply_udp_off = reply_size.curr();
	reply_size.add(sizeof(Udp_packet));
	Udp_packet &reply_udp = *reply_ip.data<Udp_packet>();
	reply_udp.src_port(Port(Dhcp_packet::BOOTPS));
	reply_udp.dst_port(Port(Dhcp_packet::BOOTPC));

	/* create mandatory DHCP fields of the reply  */
	reply_size.add(sizeof(Dhcp_packet));
	Dhcp_packet &reply_dhcp = *reply_udp.data<Dhcp_packet>();
	reply_dhcp.op(Dhcp_packet::REPLY);
	reply_dhcp.htype(Dhcp_packet::Htype::ETH);
	reply_dhcp.hlen(sizeof(Mac_address));
	reply_dhcp.hops(0);
	reply_dhcp.xid(xid);
	reply_dhcp.secs(0);
	reply_dhcp.flags(0);
	reply_dhcp.ciaddr(msg_type == Dhcp_packet::Message_type::INFORM ? client_ip : Ipv4_address());
	reply_dhcp.yiaddr(msg_type == Dhcp_packet::Message_type::INFORM ? Ipv4_address() : client_ip);
	reply_dhcp.siaddr(_router_ip());
	reply_dhcp.giaddr(Ipv4_address());
	reply_dhcp.client_mac(client_mac);
	reply_dhcp.zero_fill_sname();
	reply_dhcp.zero_fill_file();
	reply_dhcp.default_magic_cookie();

	/* append DHCP option fields to the reply */
	Dhcp_packet::Options_aggregator<Size_guard<BUF_SIZE, Dhcp_reply_buffer_too_small> >
		reply_dhcp_opts(reply_dhcp, reply_size);
	reply_dhcp_opts.append_option<Dhcp_packet::Message_type_option>(msg_type);
	reply_dhcp_opts.append_option<Dhcp_packet::Server_ipv4>(_router_ip());
	reply_dhcp_opts.append_option<Dhcp_packet::Ip_lease_time>(dhcp_srv.ip_lease_time().value / 1000 / 1000);
	reply_dhcp_opts.append_option<Dhcp_packet::Subnet_mask>(_domain.interface_attr().subnet_mask());
	reply_dhcp_opts.append_option<Dhcp_packet::Router_ipv4>(_router_ip());
	if (dhcp_srv.dns_server().valid()) {
		reply_dhcp_opts.append_option<Dhcp_packet::Dns_server_ipv4>(dhcp_srv.dns_server()); }
	reply_dhcp_opts.append_option<Dhcp_packet::Broadcast_addr>(_domain.interface_attr().broadcast_address());
	reply_dhcp_opts.append_option<Dhcp_packet::Options_end>();

	/* fill in header values that need the packet to be complete already */
	reply_udp.length(reply_size.curr() - reply_udp_off);
	reply_udp.update_checksum(reply_ip.src(), reply_ip.dst());
	reply_ip.total_length(reply_size.curr() - reply_ip_off);
	reply_ip.checksum(Ipv4_packet::calculate_checksum(reply_ip));

	/* send reply to sender of request and free reply buffer */
	log("\033[33m(", _domain, " <- router)\033[0m ", packet_log(reply_eth, _log_cfg));
	_send(reply_eth, reply_size.curr());
	_alloc.free(buf, BUF_SIZE);
}


void Interface::_release_ip_allocation(Ip_allocation &allocation)
{
	if (_config().verbose()) {
		log("Release IP allocation: ", allocation, " at ", *this);
	}
	_ip_allocations.remove(&allocation);
}


void Interface::_handle_dhcp_request(Ethernet_frame &eth,
                                     Genode::size_t  eth_size,
                                     Dhcp_packet    &dhcp)
{
	try {
		/* try to get the DHCP server config of this interface */
		Dhcp_server &dhcp_srv = _domain.dhcp_server();

		/* determine type of DHCP request */
		Dhcp_packet::Message_type const msg_type =
			dhcp.option<Dhcp_packet::Message_type_option>().value();

		try {
			/* look up existing DHCP configuration for client */
			Ip_allocation &allocation =
				_ip_allocations.find_by_mac(dhcp.client_mac());

			switch (msg_type) {
			case Dhcp_packet::Message_type::DISCOVER:

				if (allocation.bound()) {
					throw Bad_dhcp_request();

				} else {
					allocation.lifetime(_config().rtt());
					_send_dhcp_reply(dhcp_srv, eth.src(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::OFFER,
					                 dhcp.xid());
					return;
				}
			case Dhcp_packet::Message_type::REQUEST:

				if (allocation.bound()) {
					allocation.lifetime(dhcp_srv.ip_lease_time());
					_send_dhcp_reply(dhcp_srv, eth.src(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::ACK,
					                 dhcp.xid());
					return;

				} else {
					Dhcp_packet::Server_ipv4 &dhcp_srv_ip =
						dhcp.option<Dhcp_packet::Server_ipv4>();

					if (dhcp_srv_ip.value() == _router_ip()) {

						allocation.set_bound();
						allocation.lifetime(dhcp_srv.ip_lease_time());
						if (_config().verbose()) {
							log("Bind IP allocation: ", allocation,
							                    " at ", *this);
						}
						_send_dhcp_reply(dhcp_srv, eth.src(),
						                 allocation.ip(),
						                 Dhcp_packet::Message_type::ACK,
						                 dhcp.xid());
						return;

					} else {

						_release_ip_allocation(allocation);
						_destroy_ip_allocation(allocation);
						return;
					}
				}
			case Dhcp_packet::Message_type::INFORM:

				_send_dhcp_reply(dhcp_srv, eth.src(),
				                 allocation.ip(),
				                 Dhcp_packet::Message_type::ACK,
				                 dhcp.xid());
				return;

			case Dhcp_packet::Message_type::DECLINE:
			case Dhcp_packet::Message_type::RELEASE:

				_release_ip_allocation(allocation);
				_destroy_ip_allocation(allocation);
				return;

			case Dhcp_packet::Message_type::NAK:
			case Dhcp_packet::Message_type::OFFER:
			case Dhcp_packet::Message_type::ACK:
			default: throw Bad_dhcp_request();
			}
		}
		catch (Ip_allocation_tree::No_match) {

			switch (msg_type) {
			case Dhcp_packet::Message_type::DISCOVER:
				{
					Ip_allocation &allocation = *new (_alloc)
						Ip_allocation(*this, _config(),
						              dhcp_srv.alloc_ip(),
						              dhcp.client_mac(), _timer,
						              _config().rtt());

					_ip_allocations.insert(&allocation);
					if (_config().verbose()) {
						log("Offer IP allocation: ", allocation,
						                     " at ", *this);
					}
					_send_dhcp_reply(dhcp_srv, eth.src(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::OFFER,
					                 dhcp.xid());
					return;
				}
			case Dhcp_packet::Message_type::REQUEST:
			case Dhcp_packet::Message_type::DECLINE:
			case Dhcp_packet::Message_type::RELEASE:
			case Dhcp_packet::Message_type::NAK:
			case Dhcp_packet::Message_type::OFFER:
			case Dhcp_packet::Message_type::ACK:
			default: throw Bad_dhcp_request();
			}
		}
	}
	catch (Dhcp_packet::Option_not_found) {
		throw Bad_dhcp_request();
	}
}


void Interface::_handle_ip(Ethernet_frame          &eth,
                           Genode::size_t    const  eth_size,
                           Packet_descriptor const &pkt)
{
	/* read packet information */
	Ipv4_packet &ip = *new (eth.data<void>())
		Ipv4_packet(eth_size - sizeof(Ethernet_frame));

	/* try to route via transport layer rules */
	try {
		L3_protocol   const prot      = ip.protocol();
		size_t        const prot_size = ip.total_length() - ip.header_length() * 4;
		void         *const prot_base = _prot_base(prot, prot_size, ip);

		/* try handling DHCP requests before trying any routing */
		if (prot == L3_protocol::UDP) {
			Udp_packet &udp = *new (ip.data<void>())
				Udp_packet(eth_size - sizeof(Ipv4_packet));

			if (Dhcp_packet::is_dhcp(&udp)) {

			log("\033[33m(router <- ", _domain, ")\033[0m ", packet_log(eth, _log_cfg));

				/* get DHCP packet */
				Dhcp_packet &dhcp = *new (udp.data<void>())
					Dhcp_packet(eth_size - sizeof(Ipv4_packet)
					                     - sizeof(Udp_packet));

				if (dhcp.op() == Dhcp_packet::REQUEST) {
					try {
						_handle_dhcp_request(eth, eth_size, dhcp);
						return;
					}
					catch (Pointer<Dhcp_server>::Invalid) { }
				}
			}
		}
		Link_side_id  const local = { ip.src(), _src_port(prot, prot_base),
		                              ip.dst(), _dst_port(prot, prot_base) };

		/* try to route via existing UDP/TCP links */
		try {
			Link_side const &local_side = _links(prot).find_by_id(local);
			Link &link = local_side.link();
			bool const client = local_side.is_client();
			Link_side &remote_side = client ? link.server() : link.client();
			Interface &interface = remote_side.interface();
			if (_config().verbose()) {
				log("Using ", l3_protocol_name(prot), " link: ", link); }

			_adapt_eth(eth, eth_size, remote_side.src_ip(), pkt, interface);
			ip.src(remote_side.dst_ip());
			ip.dst(remote_side.src_ip());
			_src_port(prot, prot_base, remote_side.dst_port());
			_dst_port(prot, prot_base, remote_side.src_port());

			interface._pass_prot(eth, eth_size, ip, prot, prot_base, prot_size);
			_link_packet(prot, prot_base, link, client);
			return;
		}
		catch (Link_side_tree::No_match) { }

		/* try to route via forward rules */
		if (local.dst_ip == _router_ip()) {
			try {
				Forward_rule const &rule =
					_forward_rules(prot).find_by_port(local.dst_port);

				Interface &interface = rule.domain().interface().deref();
				if(_config().verbose()) {
					log("Using forward rule: ", l3_protocol_name(prot), " ", rule); }

				_adapt_eth(eth, eth_size, rule.to(), pkt, interface);
				ip.dst(rule.to());
				_nat_link_and_pass(eth, eth_size, ip, prot, prot_base, prot_size,
				                   local, interface);
				return;
			}
			catch (Forward_rule_tree::No_match) { }
		}
		/* try to route via transport and permit rules */
		try {
			Transport_rule const &transport_rule =
				_transport_rules(prot).longest_prefix_match(local.dst_ip);

			Permit_rule const &permit_rule =
				transport_rule.permit_rule(local.dst_port);

			Interface &interface = permit_rule.domain().interface().deref();
			if(_config().verbose()) {
				log("Using ", l3_protocol_name(prot), " rule: ", transport_rule,
				    " ", permit_rule); }

			_adapt_eth(eth, eth_size, local.dst_ip, pkt, interface);
			_nat_link_and_pass(eth, eth_size, ip, prot, prot_base, prot_size,
			                   local, interface);
			return;
		}
		catch (Transport_rule_list::No_match) { }
		catch (Permit_single_rule_tree::No_match) { }
	}
	catch (Interface::Bad_transport_protocol) { }

	/* try to route via IP rules */
	try {
		Ip_rule const &rule =
			_domain.ip_rules().longest_prefix_match(ip.dst());

		Interface &interface = rule.domain().interface().deref();
		if(_config().verbose()) {
			log("Using IP rule: ", rule); }

		_adapt_eth(eth, eth_size, ip.dst(), pkt, interface);
		interface._pass_ip(eth, eth_size, ip);
		return;
	}
	catch (Ip_rule_list::No_match) { }

	/* give up and drop packet */
	if (_config().verbose()) {
		log("Unroutable packet"); }
}


void Interface::_broadcast_arp_request(Ipv4_address const &ip)
{
	using Ethernet_arp = Ethernet_frame_sized<sizeof(Arp_packet)>;
	Ethernet_arp eth_arp(Mac_address(0xff), _router_mac, Ethernet_frame::Type::ARP);
	void *const eth_data = eth_arp.data<void>();
	size_t const arp_size = sizeof(eth_arp) - sizeof(Ethernet_frame);
	Arp_packet &arp = *new (eth_data) Arp_packet(arp_size);
	arp.hardware_address_type(Arp_packet::ETHERNET);
	arp.protocol_address_type(Arp_packet::IPV4);
	arp.hardware_address_size(sizeof(Mac_address));
	arp.protocol_address_size(sizeof(Ipv4_address));
	arp.opcode(Arp_packet::REQUEST);
	arp.src_mac(_router_mac);
	arp.src_ip(_router_ip());
	arp.dst_mac(Mac_address(0xff));
	arp.dst_ip(ip);
	_send(eth_arp, sizeof(eth_arp));
}


void Interface::_handle_arp_reply(Arp_packet &arp)
{
	/* do nothing if ARP info already exists */
	try {
		_arp_cache.find_by_ip(arp.src_ip());
		if (_config().verbose()) {
			log("ARP entry already exists"); }

		return;
	}
	/* create cache entry and continue handling of matching packets */
	catch (Arp_cache::No_match) {
		Ipv4_address const ip = arp.src_ip();
		_arp_cache.new_entry(ip, arp.src_mac());
		for (Arp_waiter_list_element *waiter_le = _foreign_arp_waiters.first();
		     waiter_le; )
		{
			Arp_waiter &waiter = *waiter_le->object();
			waiter_le = waiter_le->next();
			if (ip != waiter.ip()) { continue; }
			waiter.src()._continue_handle_eth(waiter.packet());
			destroy(waiter.src()._alloc, &waiter);
		}
	}
}


Ipv4_address const &Interface::_router_ip() const
{
	return _domain.interface_attr().address;
}


void Interface::_handle_arp_request(Ethernet_frame &eth,
                                    size_t const    eth_size,
                                    Arp_packet     &arp)
{
	/*
	 * We handle ARP only if it asks for the routers IP or if the router
	 * shall forward an ARP to a foreign address as gateway. The second
	 * is the case if no gateway attribute is specified (so we're the
	 * gateway) and the address is not of the same subnet like the interface
	 * attribute.
	 */
	if (arp.dst_ip() != _router_ip() &&
	    (_domain.gateway_valid() ||
	     _domain.interface_attr().prefix_matches(arp.dst_ip())))
	{
		if (_config().verbose()) {
			log("Ignore ARP request"); }

		return;
	}
	/* interchange source and destination MAC and IP addresses */
	Ipv4_address dst_ip = arp.dst_ip();
	arp.dst_ip(arp.src_ip());
	arp.dst_mac(arp.src_mac());
	eth.dst(eth.src());
	arp.src_ip(dst_ip);
	arp.src_mac(_router_mac);
	eth.src(_router_mac);

	/* mark packet as reply and send it back to its sender */
	arp.opcode(Arp_packet::REPLY);
	_send(eth, eth_size);
}


void Interface::_handle_arp(Ethernet_frame &eth, size_t const eth_size)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	size_t const arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet &arp = *new (eth.data<void>()) Arp_packet(arp_size);
	if (!arp.ethernet_ipv4()) {
		error("ARP for unknown protocol"); }

	switch (arp.opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(arp); break;
	case Arp_packet::REQUEST: _handle_arp_request(eth, eth_size, arp); break;
	default: error("unknown ARP operation"); }
}


void Interface::_ready_to_submit()
{
	while (_sink().packet_avail()) {

		Packet_descriptor const pkt = _sink().get_packet();
		if (!pkt.size()) {
			continue; }

		try { _handle_eth(_sink().packet_content(pkt), pkt.size(), pkt); }
		catch (Packet_postponed) { continue; }
		_ack_packet(pkt);
	}
}


void Interface::_continue_handle_eth(Packet_descriptor const &pkt)
{
	try { _handle_eth(_sink().packet_content(pkt), pkt.size(), pkt); }
	catch (Packet_postponed) { error("failed twice to handle packet"); }
	_ack_packet(pkt);
}


void Interface::_ready_to_ack()
{
	while (_source().ack_avail()) {
		_source().release_packet(_source().get_acked_packet()); }
}


void Interface::_destroy_ip_allocation(Ip_allocation &allocation)
{
	_domain.dhcp_server().free_ip(allocation.ip());
	destroy(_alloc, &allocation);
}


void Interface::_destroy_released_ip_allocations()
{
	while (Ip_allocation *allocation = _released_ip_allocations.first()) {
		_released_ip_allocations.remove(allocation);
		_destroy_ip_allocation(*allocation);
	}
}


void Interface::_handle_eth(void              *const  eth_base,
                            size_t             const  eth_size,
                            Packet_descriptor  const &pkt)
{
	/* do garbage collection over transport-layer links and IP allocations */
	_destroy_closed_links<Udp_link>(_closed_udp_links, _alloc);
	_destroy_closed_links<Tcp_link>(_closed_tcp_links, _alloc);
	_destroy_released_ip_allocations();

	/* inspect and handle ethernet frame */
	try {
		Ethernet_frame * const eth = new (eth_base) Ethernet_frame(eth_size);
		if (_config().verbose()) {
			log("\033[33m(router <- ", _domain, ")\033[0m ", packet_log(*eth, _log_cfg)); }

		switch (eth->type()) {
		case Ethernet_frame::Type::ARP:  _handle_arp(*eth, eth_size);     break;
		case Ethernet_frame::Type::IPV4: _handle_ip(*eth, eth_size, pkt); break;
		default: throw Bad_network_protocol(); }
	}
	catch (Ethernet_frame::No_ethernet_frame) {
		error("invalid ethernet frame"); }

	catch (Interface::Bad_network_protocol) {
		if (_config().verbose()) {
			log("unknown network layer protocol");
		}
	}
	catch (Ipv4_packet::No_ip_packet) {
		error("invalid IP packet"); }

	catch (Port_allocator_guard::Out_of_indices) {
		error("no available NAT ports"); }

	catch (Domain::No_next_hop) {
		error("can not find next hop"); }

	catch (Pointer<Interface>::Invalid) {
		error("no interface connected to domain"); }

	catch (Bad_dhcp_request) {
		error("bad DHCP request"); }

	catch (Alloc_dhcp_reply_buffer_failed) {
		error("failed to allocate buffer for DHCP reply"); }

	catch (Dhcp_reply_buffer_too_small) {
		error("DHCP reply buffer too small"); }

	catch (Dhcp_server::Alloc_ip_failed) {
		error("failed to allocate IP for DHCP client"); }
}


void Interface::_send(Ethernet_frame &eth, Genode::size_t const size)
{
	if (_config().verbose()) {
		log("\033[33m(", _domain, " <- router)\033[0m ", packet_log(eth, _log_cfg)); }
	try {
		/* copy and submit packet */
		Packet_descriptor const pkt = _source().alloc_packet(size);
		char *content = _source().packet_content(pkt);
		Genode::memcpy((void *)content, (void *)&eth, size);
		_source().submit_packet(pkt);
	}
	catch (Packet_stream_source::Packet_alloc_failed) {
		if (_config().verbose()) {
			log("Failed to allocate packet"); }
	}
}


Interface::Interface(Entrypoint        &ep,
                     Timer::Connection &timer,
                     Mac_address const  router_mac,
                     Genode::Allocator &alloc,
                     Mac_address const  mac,
                     Domain            &domain)
:
	_sink_ack(ep, *this, &Interface::_ack_avail),
	_sink_submit(ep, *this, &Interface::_ready_to_submit),
	_source_ack(ep, *this, &Interface::_ready_to_ack),
	_source_submit(ep, *this, &Interface::_packet_avail),
	_router_mac(router_mac), _mac(mac), _timer(timer), _alloc(alloc),
	_domain(domain), _log_cfg(Packet_log_config::SHORT)
{
	_log_cfg.dhcp = Packet_log_config::COMPREHENSIVE;
	if (_config().verbose()) {
		log("Interface connected ", *this);
		log("  MAC ", _mac);
		log("  Router identity: MAC ", _router_mac, " IP ",
		    _router_ip(), "/", _domain.interface_attr().prefix);
	}
	_domain.interface().set(*this);
}


void Interface::_ack_packet(Packet_descriptor const &pkt)
{
	if (!_sink().ready_to_ack()) {
		error("ack state FULL");
		return;
	}
	_sink().acknowledge_packet(pkt);
}


void Interface::_cancel_arp_waiting(Arp_waiter &waiter)
{
	warning("waiting for ARP cancelled");
	_ack_packet(waiter.packet());
	destroy(_alloc, &waiter);
}


Interface::~Interface()
{
	_domain.interface().unset();
	if (_config().verbose()) {
		log("Interface disconnected ", *this); }

	/* destroy ARP waiters */
	while (_own_arp_waiters.first()) {
		_cancel_arp_waiting(*_foreign_arp_waiters.first()->object()); }

	while (_foreign_arp_waiters.first()) {
		Arp_waiter &waiter = *_foreign_arp_waiters.first()->object();
		waiter.src()._cancel_arp_waiting(waiter); }

	/* destroy links */
	_destroy_links<Tcp_link>(_tcp_links, _closed_tcp_links, _alloc);
	_destroy_links<Udp_link>(_udp_links, _closed_udp_links, _alloc);

	/* destroy IP allocations */
	_destroy_released_ip_allocations();
	while (Ip_allocation *allocation = _ip_allocations.first()) {
		_ip_allocations.remove(allocation);
		_destroy_ip_allocation(*allocation);
	}
}


Configuration &Interface::_config() const { return _domain.config(); }


void Interface::print(Output &output) const
{
	Genode::print(output, "\"", _domain.name(), "\"");
}


/*******************
 ** Ip_allocation **
 *******************/

Ip_allocation::Ip_allocation(Interface          &interface,
                             Configuration      &config,
                             Ipv4_address const &ip,
                             Mac_address  const &mac,
                             Timer::Connection  &timer,
                             Microseconds        lifetime)
:
	_interface(interface),
	_config(config),
	_ip(ip),
	_mac(mac),
	_release_timeout(timer, *this, &Ip_allocation::_handle_release_timeout)
{
	_release_timeout.schedule(lifetime);
}


void Ip_allocation::lifetime(Microseconds lifetime)
{
	_release_timeout.schedule(lifetime);
}


bool Ip_allocation::_higher(Mac_address const &mac) const
{
	return memcmp(mac.addr, _mac.addr, sizeof(_mac.addr)) > 0;
}


Ip_allocation &Ip_allocation::find_by_mac(Mac_address const &mac)
{
	if (mac == _mac) {
		return *this; }

	Ip_allocation *const allocation = child(_higher(mac));
	if (!allocation) {
		throw Ip_allocation_tree::No_match(); }

	return allocation->find_by_mac(mac);
}


void Ip_allocation::print(Output &output) const
{
	Genode::print(output, "MAC ", _mac, " IP ", _ip);
}


void Ip_allocation::_handle_release_timeout(Duration)
{
	_interface.ip_allocation_expired(*this);
}


/************************
 ** Ip_allocation_tree **
 ************************/

Ip_allocation &
Ip_allocation_tree::find_by_mac(Mac_address const &mac) const
{
	if (!first()) {
		throw No_match(); }

	return first()->find_by_mac(mac);
}
