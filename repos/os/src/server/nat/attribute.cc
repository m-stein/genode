
#include <attribute.h>
#include <base/printf.h>

using namespace Net;
using namespace Genode;


static void cant_read_attr(char const * attr, Xml_node const & node)
{
	PERR("Can't read attribute %s in node %s", attr, node.type().string());
	class Bad_attr : public Exception { };
	throw Bad_attr();
}


Ipv4_address Net::ip_attr(char const * attr, Xml_node const & node)
{
	class Bad_ip_attr : public Exception { };
	enum { STR_SZ = 16 };
	char str[STR_SZ] = { 0 };
	try { node.attribute(attr).value(str, STR_SZ); }
	catch (...) { cant_read_attr(attr, node); }
	if (str == 0 || !strlen(str)) { throw Bad_ip_attr(); }
	return Ipv4_packet::ip_from_string(str);
}


Mac_address Net::mac_attr(char const * attr, Xml_node const & node)
{
	class Bad_mac_attr : public Exception { };
	enum { STR_SZ = 18 };
	char str[STR_SZ] = { 0 };
	try { node.attribute(attr).value(str, STR_SZ); }
	catch (...) { cant_read_attr(attr, node); }
	if (str == 0 || !strlen(str)) { throw Bad_mac_attr(); }
	return mac_from_string(str);
}


unsigned Net::uint_attr(char const * attr, Xml_node const & node)
{
	unsigned v = 0;
	try { node.attribute(attr).value(&v); }
	catch (...) { cant_read_attr(attr, node); }
	return v;
}
