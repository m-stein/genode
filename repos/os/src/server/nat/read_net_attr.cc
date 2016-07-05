
#include <read_net_attr.h>

using namespace Net;
using namespace Genode;

Ipv4_address Net::read_ip_attr(char const * attr, Xml_node & node)
{
	class Bad_ip_attr : public Exception { };
	enum { STR_SZ = 16 };
	char str[STR_SZ] = { 0 };
	node.attribute(attr).value(str, STR_SZ);
	if (str == 0 || !strlen(str)) { throw Bad_ip_attr(); }
	return Ipv4_packet::ip_from_string(str);
}


Mac_address Net::read_mac_attr(char const * attr, Xml_node const & node)
{
	class Bad_mac_attr : public Exception { };
	enum { STR_SZ = 18 };
	char str[STR_SZ] = { 0 };
	node.attribute(attr).value(str, STR_SZ);
	if (str == 0 || !strlen(str)) { throw Bad_mac_attr(); }
	return mac_from_string(str);
}
