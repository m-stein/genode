
#include <address_node.h>
#include <component.h>
#include <net/tcp.h>

using namespace Net;
using namespace Genode;


bool Route_node::matches(Ipv4_address ip_addr)
{
	if (memcmp(&ip_addr.addr, _ip_addr.addr, _prefix_size)) { return false; }
	if ((ip_addr.addr[_prefix_size] & _netmask.addr[_prefix_size]) !=
	    _ip_addr.addr[_prefix_size]) { return false; }
	return true;
}


void Route_node::dump()
{
	PINF("IP Route %u.%u.%u.%u:%u > %s via %u.%u.%u.%u",
	      _ip_addr.addr[0],
	      _ip_addr.addr[1],
	      _ip_addr.addr[2],
	      _ip_addr.addr[3],
	      _prefix_width,
	      _interface.string(),
	      _gateway.addr[0],
	      _gateway.addr[1],
	      _gateway.addr[2],
	      _gateway.addr[3]);
}


Route_node::Route_node
(
	Ipv4_address ip_addr, Ipv4_address netmask, Ipv4_address gateway,
	char const * interface, size_t interface_size)
:
	_ip_addr(ip_addr), _netmask(netmask), _gateway(gateway),
	_interface(interface, interface_size)
{
	size_t i = 0;
	for (; i < sizeof(_netmask.addr); i++) {
		if (_netmask.addr[i] != (uint8_t)~0) { break; }
		_prefix_size  += 1;
		_prefix_width += 8;
	}
	for (size_t j = 0; j < 8; j++) {
		if ((_netmask.addr[i] >> j) & 1) {
			_prefix_width += 8 - j;
			break;
		}
	}
	dump();
}


Route_node * Route_list::longest_prefix_match(Ipv4_address ip_addr)
{
	for (Route_node * curr = first(); curr; curr = curr->next()) {
		if (curr->matches(ip_addr)) { return curr; }
	}
	return nullptr;
}


void Route_list::insert(Route_node * route)
{
	Route_node * behind = nullptr;
	for (Route_node * curr = first(); curr; curr = curr->next()) {
		if (route->prefix_width() >= curr->prefix_width()) { break; }
		behind = curr;
	}
	Genode::List<Route_node>::insert(route, behind);
}

Arp_waiter::Arp_waiter
(
	Packet_handler * handler, Ipv4_address ip_addr,
	Ethernet_frame * const eth, Genode::size_t const eth_size, Packet_descriptor * packet)
:
	_handler(handler), _ip_addr(ip_addr), _eth(eth), _eth_size(eth_size), _packet(packet)
{ }

bool Arp_waiter::new_arp_node(Arp_node * arp_node)
{
	if (!(arp_node->ip() == _ip_addr)) { return false; }
	_handler->continue_handle_ethernet(_eth, _eth_size, _packet);
	return true;
}


Proxy_role::Proxy_role
(
	uint16_t client_port, uint16_t proxy_port, Ipv4_address client_ip,
	Ipv4_address proxy_ip, Packet_handler * client)
:
	_client_port(client_port), _proxy_port(proxy_port), _client_ip(client_ip),
	_proxy_ip(proxy_ip), _client(client)
{ }


bool Proxy_role::matches_client
(
	Ipv4_address client_ip, uint16_t client_port)
{
	return client_ip == _client_ip && client_port == _client_port;
}


bool Proxy_role::matches_proxy
(
	Ipv4_address proxy_ip, uint16_t proxy_port)
{
	return proxy_ip == _proxy_ip && proxy_port == _proxy_port;
}


void Proxy_role::tcp_packet(Ipv4_packet * const ip, Tcp_packet * const tcp)
{
	PERR("tcp_packet");
	bool from_client;
	if (tcp->src_port() == _client_port) { from_client = true; PERR("from_client 1");}
	else {                                 from_client = false; PERR("from_client 0");}
	if (tcp->fin()) {
		if (from_client) { _client_fin = true; PERR("client_fin"); }
		else {             _other_fin  = true; PERR("other_fin"); }
	}
	if (tcp->ack()) {
		if (from_client  && _other_fin)  { _other_fin_acked  = true; PERR("other_fin_acked"); }
		if (!from_client && _client_fin) { _client_fin_acked = true; PERR("client_fin_acked"); }
		if (_other_fin_acked && _client_fin_acked) { _destroy = true; PERR("Destroy!"); }
	}
}
