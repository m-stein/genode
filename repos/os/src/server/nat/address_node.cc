
#include <address_node.h>
#include <component.h>
#include <net/tcp.h>
#include <attribute.h>

using namespace Net;
using namespace Genode;

static const bool verbose = false;

Port_node::Port_node
(
	uint16_t nr, char const * label, size_t label_size, Ipv4_address via,
	char const * type)
:
	_nr(nr), _label(label, label_size), _via(via)
{
	if (verbose) { _dump(type); }
}


void Port_node::_dump(char const * type)
{
	PLOG("%s port route %u %s %u.%u.%u.%u",
		type, _nr, _label.string(),
		_via.addr[0],
		_via.addr[1],
		_via.addr[2],
		_via.addr[3]
	);
}


Port_node * Port_node::find_by_nr(uint16_t nr)
{
	if (nr == _nr) { return this; }
	bool side = nr > _nr;
	Port_node *c = Avl_node<Port_node>::child(side);
	return c ? c->find_by_nr(nr) : 0;
}


bool Route_node::matches(Ipv4_address ip_addr)
{
	if (memcmp(&ip_addr.addr, _ip_addr.addr, _prefix_bytes)) { return false; }
	if (!_prefix_tail) { return true; }
	uint8_t ip_tail = ip_addr.addr[_prefix_bytes] & _prefix_tail;
	if (ip_tail != _ip_addr.addr[_prefix_bytes]) { return false; }
	return true;
}


void Route_node::dump()
{
	PLOG("IP Route %u.%u.%u.%u:%u %u %x > %s via %u.%u.%u.%u",
	      _ip_addr.addr[0],
	      _ip_addr.addr[1],
	      _ip_addr.addr[2],
	      _ip_addr.addr[3],
	      _prefix,
	      _prefix_bytes,
	      _prefix_tail,
	      _label.string(),
	      _via.addr[0],
	      _via.addr[1],
	      _via.addr[2],
	      _via.addr[3]);
}


void Route_node::_read_tcp_port
(
	Xml_node & port, Allocator * alloc)
{
	uint16_t const nr = uint_attr("nr", port);
	char const * label = port.attribute("label").value_base();
	size_t label_size = port.attribute("label").value_size();
	Ipv4_address via;
	try { via = ip_attr("via", port); } catch (Bad_ip_attr) { }
	Port_node * port_node = new (alloc)
		Port_node(nr, label, label_size, via, "TCP");
	_tcp_port_tree.insert(port_node);
}

void Route_node::_read_udp_port
(
	Xml_node & port, Allocator * alloc)
{
	uint16_t const nr = uint_attr("nr", port);
	char const * label = port.attribute("label").value_base();
	size_t label_size = port.attribute("label").value_size();
	Ipv4_address via;
	try { via = ip_attr("via", port); } catch (Bad_ip_attr) { }
	Port_node * port_node = new (alloc)
		Port_node(nr, label, label_size, via, "UDP");
	_udp_port_tree.insert(port_node);
	PLOG("UDP Port Route %u %s %u.%u.%u.%u", port_node->nr(), port_node->label().string(),
		port_node->via().addr[0],
		port_node->via().addr[1],
		port_node->via().addr[2],
		port_node->via().addr[3]
	);
}



Route_node::Route_node
(
	Ipv4_address ip_addr, uint8_t prefix, Ipv4_address via,
	char const * label, size_t label_size, Allocator * alloc,
	Xml_node & route)
:
	_ip_addr(ip_addr), _prefix(prefix), _prefix_bytes(_prefix / 8),
	_prefix_tail(~(((uint8_t)~0) >> (_prefix - (_prefix_bytes * 8)))),
	_via(via), _label(label, label_size)
{
	if (verbose) { dump(); }
	try {
		Xml_node port = route.sub_node("tcp-port");
		for (; ; port = port.next("tcp-port")) { _read_tcp_port(port, alloc); }
	} catch (Xml_node::Nonexistent_sub_node) { }
	try {
		Xml_node port = route.sub_node("udp-port");
		for (; ; port = port.next("udp-port")) { _read_udp_port(port, alloc); }
	} catch (Xml_node::Nonexistent_sub_node) { }
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
		if (route->prefix() >= curr->prefix()) { break; }
		behind = curr;
	}
	Genode::List<Route_node>::insert(route, behind);
}

Arp_waiter::Arp_waiter
(
	Packet_handler * handler, Ipv4_address ip_addr,
	Ethernet_frame * const eth, size_t const eth_size, Packet_descriptor * packet)
:
	_handler(handler), _ip_addr(ip_addr), _eth(eth), _eth_size(eth_size), _packet(packet)
{ }

bool Arp_waiter::new_arp_node(Arp_node * arp_node)
{
	if (!(arp_node->ip() == _ip_addr)) { return false; }
	_handler->continue_handle_ethernet(_eth, _eth_size, _packet);
	return true;
}
