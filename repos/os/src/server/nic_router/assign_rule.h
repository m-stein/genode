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

#ifndef _ASSIGN_RULE_H_
#define _ASSIGN_RULE_H_

/* local includes */
#include <avl_tree.h>

/* Genode includes */
#include <util/avl_tree.h>
#include <net/ipv4.h>
#include <net/mac_address.h>

namespace Genode { class Xml_node; }

namespace Net {

	class Dhcp_server;

	class Assign_rule;
	class Assign_rule_tree;
}


class Net::Assign_rule : public Genode::Avl_node<Assign_rule>
{
	private:

		Mac_address  const _mac;
		Ipv4_address const _ip;

		bool _higher(Mac_address const &mac) const;

	public:

		struct Invalid : Genode::Exception { };

		Assign_rule(Dhcp_server            &dhcp_server,
		            Genode::Xml_node const &node);

		Assign_rule const &find_by_mac(Mac_address const &mac) const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Assign_rule *rule) { return _higher(rule->mac()); }


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address const &ip()  const { return _ip; }
		Mac_address  const &mac() const { return _mac; }
};


struct Net::Assign_rule_tree : Avl_tree<Assign_rule>
{
	struct No_match : Genode::Exception { };

	Assign_rule const &find_by_mac(Mac_address const &mac) const;
};

#endif /* _ASSIGN_RULE_H_ */
