/*
 * \brief  Reflects an effective policy configuration node
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <configuration.h>
#include <protocol_name.h>

/* Genode includes */
#include <util/xml_node.h>
#include <base/allocator.h>
#include <base/log.h>

using namespace Net;
using namespace Genode;


/***********************
 ** Policy_avl_member **
 ***********************/

Policy_avl_member::Policy_avl_member(Genode::Session_label const &label,
                                     Net::Policy                 &policy)
:
	Avl_string_base(label.string()), _policy(policy)
{ }


/*****************
 ** Policy_base **
 *****************/

Policy_base::Policy_base(Xml_node const &node)
:
	_label(Cstring(node.attribute("label").value_base(),
	               node.attribute("label").value_size()))
{ }


/************
 ** Policy **
 ************/

void Policy::_read_forward_rules(Cstring  const    &protocol,
                                 Policy_tree       &policies,
                                 Xml_node const    &node,
                                 char     const    *type,
                                 Forward_rule_tree &rules)
{
	node.for_each_sub_node(type, [&] (Xml_node const &node) {
		try {
			Forward_rule &rule = *new (_alloc) Forward_rule(policies, node);
			rules.insert(&rule);
			if (_config.verbose()) {
				log("  Forward rule: ", protocol, " ", rule); }
		}
		catch (Rule::Invalid) { warning("invalid forward rule"); }
	});
}


void Policy::_read_transport_rules(Cstring  const      &protocol,
                                   Policy_tree         &policies,
                                   Xml_node const      &node,
                                   char     const      *type,
                                   Transport_rule_list &rules)
{
	node.for_each_sub_node(type, [&] (Xml_node const &node) {
		try {
			rules.insert(*new (_alloc) Transport_rule(policies, node, _alloc,
			                                          protocol, _config));
		}
		catch (Rule::Invalid) { warning("invalid transport rule"); }
	});
}


void Policy::print(Output &output) const
{
	Genode::print(output, "\"", _label, "\"");
}


Policy::Policy(Configuration &config, Xml_node const &node, Allocator &alloc)
:
	Policy_base(node), _avl_member(_label, *this), _config(config),
	_node(node), _alloc(alloc),
	_interface_attr(node.attribute_value("interface", Ipv4_address_prefix())),
	_gateway(node.attribute_value("gateway", Ipv4_address())),
	_gateway_valid(_gateway.valid())
{
	if (_label == Session_label() || !_interface_attr.valid() ||
	    (_gateway_valid && !_interface_attr.prefix_matches(_gateway)))
	{
		throw Invalid();
	}
}


void Policy::create_rules(Policy_tree &policies)
{
	/* read forward rules */
	_read_forward_rules(tcp_name(), policies, _node, "tcp-forward",
	                    _tcp_forward_rules);
	_read_forward_rules(udp_name(), policies, _node, "udp-forward",
	                    _udp_forward_rules);

	/* read UDP and TCP rules */
	_read_transport_rules(tcp_name(), policies, _node, "tcp", _tcp_rules);
	_read_transport_rules(udp_name(), policies, _node, "udp", _udp_rules);

	/* read NAT rules */
	_node.for_each_sub_node("nat", [&] (Xml_node const &node) {
		try {
			_nat_rules.insert(
				new (_alloc) Nat_rule(policies, _tcp_port_alloc,
				                      _udp_port_alloc, node));
		}
		catch (Rule::Invalid) { warning("invalid NAT rule"); }
	});
	/* read IP rules */
	_node.for_each_sub_node("ip", [&] (Xml_node const &node) {
		try { _ip_rules.insert(*new (_alloc) Ip_rule(policies, node)); }
		catch (Rule::Invalid) { warning("invalid IP rule"); }
	});
}


Ipv4_address const &Policy::next_hop(Ipv4_address const &ip) const
{
	if (_interface_attr.prefix_matches(ip)) { return ip; }
	if (_gateway_valid) { return _gateway; }
	throw No_next_hop();
}


/*****************
 ** Policy_tree **
 *****************/

Net::Policy &Policy_tree::policy(Avl_string_base const &node)
{
	return static_cast<Policy_avl_member const *>(&node)->policy();
}

Net::Policy &Policy_tree::find_by_label(Session_label label)
{
	if (label == Session_label() || !first()) {
		throw No_match(); }

	Avl_string_base *node = first()->find_by_name(label.string());
	if (!node) {
		throw No_match(); }

	return policy(*node);
}
