
#include <os/config.h>

#include <vlan.h>
#include <attribute.h>
#include <interface.h>

using namespace Net;
using namespace Genode;

static bool const verbose = 1;

void Vlan::_read_ports(Xml_node & route, char const * name, Port_allocator & port_alloc)
{
	try {
		Xml_node port = route.sub_node(name);
		for (; ; port = port.next(name)) {
			uint16_t nr;
			try { nr = uint_attr("nr", port); }
			catch (Bad_uint_attr) { continue; }
			try { port_alloc.alloc_index(nr); }
			catch (Port_allocator::Already_allocated) { continue; }
			if (verbose) { log("Reserve ", name, " ", nr); }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }
}


Vlan::Vlan(Port_allocator & tcp_port_alloc, Port_allocator & udp_port_alloc)
:
	_rtt_sec(uint_attr("rtt_sec", config()->xml_node()))
{
	try {
		Xml_node policy = config()->xml_node().sub_node("policy");
		for (; ; policy = policy.next("policy")) {
			try {
				Xml_node route = policy.sub_node("route");
				for (; ; route = route.next("route")) {
					_read_ports(route, "tcp-port", tcp_port_alloc);
					_read_ports(route, "udp-port", udp_port_alloc);
				}
			} catch (Xml_node::Nonexistent_sub_node) { }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }
}


Interface * Interface_tree::find_by_label(char const * label)
{
	if (!strcmp(label, "")) { return nullptr; }
	Interface * interface = static_cast<Interface *>(first());
	if (!interface) { return nullptr; }
	interface = static_cast<Interface *>(interface->find_by_name(label));
	return interface;
}
