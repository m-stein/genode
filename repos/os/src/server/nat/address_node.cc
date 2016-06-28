
#include <address_node.h>

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
	PINF("IP Route %u.%u.%u.%u:%u > %u.%u.%u.%u",
	      _ip_addr.addr[0],
	      _ip_addr.addr[1],
	      _ip_addr.addr[2],
	      _ip_addr.addr[3],
	      _prefix_width,
	      _gateway.addr[0],
	      _gateway.addr[1],
	      _gateway.addr[2],
	      _gateway.addr[3]);
}


Route_node::Route_node(Ipv4_address ip_addr, Ipv4_address netmask,
                       Ipv4_address gateway)
:
	_ip_addr(ip_addr), _netmask(netmask), _gateway(gateway)
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
