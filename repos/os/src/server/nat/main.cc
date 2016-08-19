
/* Genode */
#include <base/env.h>
#include <cap_session/connection.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic/xml_node.h>
#include <os/server.h>

/* local includes */
#include <component.h>
#include <attribute.h>
#include <port_allocator.h>

using namespace Genode;
using namespace Net;

struct Main
{
	private:

		Port_allocator       _tcp_port_alloc;
		Port_allocator       _udp_port_alloc;
		Server::Entrypoint & _ep;
		Vlan                 _vlan;
		Uplink               _uplink;
		Net::Root            _root;

		void _handle_config();

	public:

		Main(Server::Entrypoint & ep);
};


void Main::_handle_config()
{
	try {
		Nic::Mac_address mac;
		config()->xml_node().attribute("mac").value(&mac);
		memcpy(&Mac_allocator::mac_addr_base, &mac,
					   sizeof(Mac_allocator::mac_addr_base));
	} catch(...) {}
}


Main::Main(Server::Entrypoint & ep)
:
	_ep(ep), _vlan(_tcp_port_alloc, _udp_port_alloc),
	_uplink(_ep, _vlan, _tcp_port_alloc, _udp_port_alloc),
	_root(_ep, _uplink, env()->heap(), _uplink.nat_mac(), _tcp_port_alloc,
	      _udp_port_alloc)
{
	_handle_config();
	env()->parent()->announce(ep.manage(_root));
}


/************
 ** Server **
 ************/

namespace Server
{
	char const * name() { return "nat_ep"; }
	size_t stack_size() { return 2048 * sizeof(addr_t); }
	void construct(Entrypoint & ep) { static Main nat(ep); }
}
