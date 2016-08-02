
#ifndef _ATTRIBUTE_H_
#define _ATTRIBUTE_H_

#include <util/xml_node.h>
#include <net/ipv4.h>
#include <net/ethernet.h>

namespace Net
{
	class Bad_attr : public Genode::Exception { };

	void ip_prefix_attr(
		char const * attr, Genode::Xml_node const & node, Ipv4_address & ip,
		Genode::uint8_t & prefix);

	Ipv4_address ip_attr(char const * attr, Genode::Xml_node const & node);
	Mac_address mac_attr(char const * attr, Genode::Xml_node const & node);
	unsigned uint_attr(char const * attr, Genode::Xml_node const & node);
}

#endif /* _ATTRIBUTE_H_ */
