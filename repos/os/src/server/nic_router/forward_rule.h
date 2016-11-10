/*
 * \brief  Rule for forwarding a TCP/UDP port of the router to an interface
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FORWARD_RULE_H_
#define _FORWARD_RULE_H_

/* local includes */
#include <leaf_rule.h>

/* Genode includes */
#include <util/avl_tree.h>
#include <net/ipv4.h>

namespace Net {

	class Forward_rule;
	class Forward_rule_tree;
	class Forward_link;
	class Forward_link_tree;
}


class Net::Forward_rule : public Leaf_rule,
                          public Genode::Avl_node<Forward_rule>
{
	private:

		Genode::uint8_t const _port;
		Ipv4_address    const _to;

	public:

		Forward_rule(Policy_tree &policies, Genode::Xml_node const &node);

		Forward_rule const &find_by_port(Genode::uint8_t const port) const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Forward_rule *rule) { return rule->_port > _port; }


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address const &to() const { return _to; }
};


struct Net::Forward_rule_tree : Genode::Avl_tree<Forward_rule>
{
	Forward_rule const &find_by_port(Genode::uint8_t const port) const;
};

#endif /* _FORWARD_RULE_H_ */
