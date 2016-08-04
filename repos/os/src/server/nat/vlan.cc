
#include <os/config.h>

#include <vlan.h>
#include <attribute.h>

using namespace Net;
using namespace Genode;

Vlan::Vlan(Port_allocator & port_alloc)
:
	_rtt_sec(uint_attr("rtt_sec", config()->xml_node()))
{
	/* reserve ports that are used for port forwarding */
	try {
		Xml_node policy = config()->xml_node().sub_node("policy");
		for (; ; policy = policy.next("policy")) {
			try {
				Xml_node route = config()->xml_node().sub_node("route");
				for (; ; route = route.next("route")) {
					try {
						Xml_node port = config()->xml_node().sub_node("port");
						for (; ; port = port.next("port")) {

							uint8_t const nr = uint_attr("nr", port);
							PLOG("Reserve port %u", nr);
							try { port_alloc.alloc_index(nr); }
							catch (...) { }
						}
					} catch (Xml_node::Nonexistent_sub_node) { }
				}
			} catch (Xml_node::Nonexistent_sub_node) { }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }
}
