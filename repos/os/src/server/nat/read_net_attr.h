
#include <util/xml_node.h>
#include <net/ipv4.h>
#include <net/ethernet.h>

#ifndef _READ_NET_ATTR_H_
#define _READ_NET_ATTR_H_

namespace Net
{
	Ipv4_address read_ip_attr(char const * attr, Genode::Xml_node & node);
	Mac_address read_mac_attr(char const * attr, Genode::Xml_node const & node);
}

#endif /* READ_NET_ATTR */
