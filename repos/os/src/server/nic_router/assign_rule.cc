/*
 * \brief  Rule for assigning an IP address to a peer with a certain MAC
 * \author Martin Stein
 * \date   2019-03-19
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <assign_rule.h>
#include <dhcp_server.h>

/* Genode includes */
#include <util/xml_node.h>

using namespace Net;
using namespace Genode;


/******************
 ** Assign_rule **
 ******************/

void Assign_rule::print(Output &output) const
{
	Genode::print(output, "mac ", _mac, " ip ", _ip);
}


Assign_rule::Assign_rule(Dhcp_server    &dhcp_server,
                         Xml_node const &node)
:
	_mac { node.attribute_value("mac", Mac_address()) },
	_ip  { node.attribute_value("ip", Ipv4_address()) }
{
	if (_mac == Mac_address() || !_ip.valid()) {
		throw Invalid(); }

	try { dhcp_server.alloc_ip(_ip); }
	catch (Dhcp_server::Alloc_ip_failed) {
		throw Invalid(); }
}


Assign_rule const &Assign_rule::find_by_mac(Mac_address const &mac) const
{
	if (mac == _mac) {
		return *this; }

	Assign_rule *const rule =
		Avl_node<Assign_rule>::child(_higher(mac));

	if (!rule) {
		throw Assign_rule_tree::No_match(); }

	return rule->find_by_mac(mac);
}


bool Assign_rule::_higher(Mac_address const &mac) const
{
	return memcmp(&mac, &_mac, sizeof(Mac_address)) > 0;
}


/***********************
 ** Assign_rule_tree **
 ***********************/

Assign_rule const &Assign_rule_tree::find_by_mac(Mac_address const &mac) const
{
	if (!first()) {
		throw No_match(); }

	return first()->find_by_mac(mac);
}
