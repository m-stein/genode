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

#ifndef _LEAF_RULE_H_
#define _LEAF_RULE_H_

/* local includes */
#include <rule.h>

namespace Genode { class Xml_node; }

namespace Net {

	class Policy;
	class Policy_tree;
	class Leaf_rule;
}

class Net::Leaf_rule : public Rule
{
	protected:

		Net::Policy &_policy;

		static Policy &_find_policy(Policy_tree            &policies,
		                            Genode::Xml_node const &node);

	public:

		Leaf_rule(Policy_tree &policies, Genode::Xml_node const &node);


		/***************
		 ** Accessors **
		 ***************/

		Net::Policy &policy() const { return _policy; }
};

#endif /* _LEAF_RULE_H_ */
