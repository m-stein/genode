
#include <vlan.h>
#include <os/config.h>

using namespace Net;
using namespace Genode;


Ipv4_address Vlan::_read_ip_attr(char const * attr, Xml_node & node)
{
	class Bad_ip_attr : Exception { };
	enum { STR_SZ = 16 };
	char str[STR_SZ] = { 0 };
	node.attribute(attr).value(str, STR_SZ);
	if (str == 0 || !strlen(str)) { throw Bad_ip_attr(); }
	return Ipv4_packet::ip_from_string(str);
}


void Vlan::_read_route(Xml_node & route_xn)
{
	Ipv4_address ip = _read_ip_attr("ip_addr", route_xn);
	Ipv4_address nm = _read_ip_attr("netmask", route_xn);
	Ipv4_address gw = _read_ip_attr("gateway", route_xn);
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
