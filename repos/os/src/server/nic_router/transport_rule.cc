/*
 * \brief  Rule for allowing direct TCP/UDP traffic between two interfaces
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/xml_node.h>
#include <base/allocator.h>
#include <base/log.h>

/* local includes */
#include <transport_rule.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


Permit_any_rule *
Transport_rule::_read_permit_any_rule(Domain_dict    &domains,
                                      Xml_node const  node,
                                      Allocator      &alloc)
{
	Permit_any_rule *ptr { };
	node.with_optional_sub_node("permit-any", [&] (Xml_node const &sub_node) {
		ptr = new (alloc) Permit_any_rule(domains, sub_node); });
	return ptr;
}


Transport_rule::Transport_rule(Domain_dict               &domains,
                               Ipv4_address_prefix const &dst,
                               Xml_node            const  node,
                               Allocator                 &alloc,
                               Cstring             const  &protocol,
                               Configuration             &config,
                               Domain              const  &domain)
:
	Direct_rule(dst),
	_alloc(alloc),
	_permit_any_rule_ptr(_read_permit_any_rule(domains, node, alloc))
{
	/* skip specific permit rules if all ports are permitted anyway */
	if (_permit_any_rule_ptr) {
		if (config.verbose()) {
			log("[", domain, "] ", protocol, " permit-any rule: ", *_permit_any_rule_ptr);
			log("[", domain, "] ", protocol, " rule: dst ", _dst);
		}
		return;
	}
	/* read specific permit rules */
	node.for_each_sub_node("permit", [&] (Xml_node const node) {
		Permit_single_rule &rule = *new (alloc)
			Permit_single_rule(domains, node);

		_permit_single_rules.insert(&rule);
		if (config.verbose()) {
			log("[", domain, "] ", protocol, " permit rule: ", rule); }
	});
}


Transport_rule::~Transport_rule()
{
	_permit_single_rules.destroy_each(_alloc);
	if (_permit_any_rule_ptr)
		destroy(_alloc, _permit_any_rule_ptr);
}
