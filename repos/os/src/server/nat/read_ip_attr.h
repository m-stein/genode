
#include <util/xml_node.h>
#include <net/ipv4.h>

#ifndef _READ_IP_ATTR_H_
#define _READ_IP_ATTR_H_

namespace Net {
	Ipv4_address read_ip_attr(char const * attr, Genode::Xml_node & node); }

#endif /* READ_IP_ATTR */
