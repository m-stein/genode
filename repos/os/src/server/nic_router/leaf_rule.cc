/*
 * \brief  Routing rule that defines a target interface
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
#include <leaf_rule.h>
#include <policy.h>

using namespace Net;
using namespace Genode;


Net::Policy &Leaf_rule::_find_policy(Policy_tree    &policies,
                                     Xml_node const &node)
{
	try {
		return policies.find_by_label(
			Cstring(node.attribute("label").value_base(),
			        node.attribute("label").value_size()));
	}
	catch (Policy_tree::No_match) { throw Invalid(); }
}


Leaf_rule::Leaf_rule(Policy_tree &policies, Xml_node const &node)
:
	_policy(_find_policy(policies, node))
{ }
