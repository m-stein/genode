
#include <os/config.h>

#include <vlan.h>
#include <attribute.h>

using namespace Net;
using namespace Genode;

Vlan::Vlan() : _rtt_sec(uint_attr("rtt_sec", config()->xml_node())) { }
