
#include <attribute.h>
#include <base/printf.h>

using namespace Net;
using namespace Genode;


void Net::ip_prefix_attr
(
	char const * attr, Xml_node const & node, Ipv4_address & ip,
	uint8_t & prefix)
{
	enum { STR_SZ = 19 };
	char str[STR_SZ] = { 0 };
	try { node.attribute(attr).value(str, STR_SZ); }
	catch (...) { throw Bad_ip_prefix_attr(); }
	if (str == 0 || !strlen(str)) { throw Bad_ip_prefix_attr(); }
	ip = Ipv4_packet::ip_from_string(str);
	unsigned i = 0;
	for (; i < STR_SZ && str[i] != '/'; i++) { }
	if (i == STR_SZ) { throw Bad_ip_prefix_attr(); }
	if (!ascii_to_unsigned(&str[i+1], prefix, 10)) {
		throw Bad_ip_prefix_attr(); }
}


Ipv4_address Net::ip_attr(char const * attr, Xml_node const & node)
{
	enum { STR_SZ = 16 };
	char str[STR_SZ] = { 0 };
	try { node.attribute(attr).value(str, STR_SZ); }
	catch (...) { throw Bad_ip_attr(); }
	if (str == 0 || !strlen(str)) { throw Bad_ip_attr(); }
	return Ipv4_packet::ip_from_string(str);
}


Mac_address Net::mac_attr(char const * attr, Xml_node const & node)
{
	enum { STR_SZ = 18 };
	char str[STR_SZ] = { 0 };
	try { node.attribute(attr).value(str, STR_SZ); }
	catch (...) { throw Bad_mac_attr(); }
	if (str == 0 || !strlen(str)) { throw Bad_mac_attr(); }
	return mac_from_string(str);
}


unsigned Net::uint_attr(char const * attr, Xml_node const & node)
{
	unsigned v = 0;
	try { node.attribute(attr).value(&v); }
	catch (...) { throw Bad_uint_attr(); }
	return v;
}
