
#include <os/config.h>

#include <vlan.h>
#include <read_ip_attr.h>

using namespace Net;
using namespace Genode;

void Vlan::_read_route(Xml_node & route_xn)
{
	Ipv4_address ip = read_ip_attr("ip_addr", route_xn);
	Ipv4_address nm = read_ip_attr("netmask", route_xn);
	Ipv4_address gw = read_ip_attr("gateway", route_xn);
	char const * in = route_xn.attribute("interface").value_base();
	size_t in_sz    = route_xn.attribute("interface").value_size();
	Route_node * route = new (env()->heap()) Route_node(ip, nm, gw, in, in_sz);
	_ip_routes.insert(route);
}


Vlan::Vlan()
{
	try {
		Xml_node route = config()->xml_node().sub_node("route");
		for (; ; route = route.next("route")) { _read_route(route); }
	} catch (Xml_node::Nonexistent_sub_node) { }
}
