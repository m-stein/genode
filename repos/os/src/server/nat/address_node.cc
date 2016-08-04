
#include <address_node.h>
#include <component.h>
#include <net/tcp.h>
#include <attribute.h>

using namespace Net;
using namespace Genode;


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
	      _interface.string(),
	      _gateway.addr[0],
	      _gateway.addr[1],
	      _gateway.addr[2],
	      _gateway.addr[3]);
}


void Route_node::_read_port
(
	Xml_node & port, Allocator * alloc)
{
	uint16_t const nr = uint_attr("nr", port);
	char const * label = port.attribute("label").value_base();
	size_t label_size = port.attribute("label").value_size();
	Port_node * port_node = new (alloc) Port_node(nr, label, label_size);
	_port_tree.insert(port_node);
	PLOG("Port Route %u %s", port_node->nr(), port_node->label().string());
}



Route_node::Route_node
(
	Ipv4_address ip_addr, uint8_t prefix, Ipv4_address gateway,
	char const * interface, size_t interface_size, Allocator * alloc,
	Xml_node & route)
:
	_ip_addr(ip_addr), _prefix(prefix), _prefix_bytes(_prefix / 8),
	_prefix_tail(~(((uint8_t)~0) >> (_prefix - (_prefix_bytes * 8)))),
	_gateway(gateway), _interface(interface, interface_size)
{
	dump();
	try {
		Xml_node port = route.sub_node("port");
		for (; ; port = port.next("port")) { _read_port(port, alloc); }
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
