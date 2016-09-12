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

#ifndef _POLICY_H_
#define _POLICY_H_

/* local includes */
#include <forward_rule.h>
#include <transport_rule.h>
#include <nat_rule.h>
#include <ip_rule.h>
#include <port_allocator.h>
#include <pointer.h>

/* Genode includes */
#include <util/avl_string.h>
#include <util/xml_node.h>
#include <base/session_label.h>

namespace Genode { class Allocator; }

namespace Net {

	class Interface;
	class Configuration;
	class Policy_avl_member;
	class Policy_base;
	class Policy;
	class Policy_tree;
}


class Net::Policy_avl_member : public Genode::Avl_string_base
{
	private:

		Net::Policy &_policy;

	public:

		Policy_avl_member(Genode::Session_label const &label,
		                  Net::Policy                 &policy);


		/***************
		 ** Accessors **
		 ***************/

		Net::Policy &policy() const { return _policy; }
};


class Net::Policy_base
{
	protected:

		Genode::Session_label const _label;

		Policy_base(Genode::Xml_node const &node);
};


class Net::Policy : public Policy_base
{
	private:

		Policy_avl_member       _avl_member;
		Configuration          &_config;
		Genode::Xml_node        _node;
		Genode::Allocator      &_alloc;
		Ipv4_address_prefix     _interface_attr;
		Ipv4_address const      _gateway;
		bool         const      _gateway_valid;
		Ip_rule_list            _ip_rules;
		Forward_rule_tree       _tcp_forward_rules;
		Forward_rule_tree       _udp_forward_rules;
		Transport_rule_list     _tcp_rules;
		Transport_rule_list     _udp_rules;
		Port_allocator          _tcp_port_alloc;
		Port_allocator          _udp_port_alloc;
		Nat_rule_tree           _nat_rules;
		Pointer<Interface>      _interface;

		void _read_forward_rules(Genode::Cstring  const &protocol,
		                         Policy_tree            &policies,
		                         Genode::Xml_node const &node,
		                         char             const *type,
		                         Forward_rule_tree      &rules);

		void _read_transport_rules(Genode::Cstring  const &protocol,
		                           Policy_tree            &policies,
		                           Genode::Xml_node const &node,
		                           char             const *type,
		                           Transport_rule_list    &rules);

	public:

		struct Invalid     : Genode::Exception { };
		struct No_next_hop : Genode::Exception { };

		Policy(Configuration          &config,
		       Genode::Xml_node const &node,
		       Genode::Allocator      &alloc);

		void create_rules(Policy_tree &policies);

		Ipv4_address const &next_hop(Ipv4_address const &ip) const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Genode::Session_label const &label()             { return _label; }
		Ip_rule_list                &ip_rules()          { return _ip_rules; }
		Forward_rule_tree           &tcp_forward_rules() { return _tcp_forward_rules; }
		Forward_rule_tree           &udp_forward_rules() { return _udp_forward_rules; }
		Transport_rule_list         &tcp_rules()         { return _tcp_rules; }
		Transport_rule_list         &udp_rules()         { return _udp_rules; }
		Nat_rule_tree               &nat_rules()         { return _nat_rules; }
		Ipv4_address_prefix         &interface_attr()    { return _interface_attr; }
		Pointer<Interface>          &interface()         { return _interface; }
		Configuration               &config() const      { return _config; }
		Policy_avl_member           &avl_member()        { return _avl_member; }
};


struct Net::Policy_tree : Genode::Avl_tree<Genode::Avl_string_base>
{
	using Avl_tree = Genode::Avl_tree<Genode::Avl_string_base>;

	static Net::Policy &policy(Genode::Avl_string_base const &node);

	Net::Policy &find_by_label(Genode::Session_label label);

	template <typename FUNC>
	void for_each(FUNC && functor) const {
		Avl_tree::for_each([&] (Genode::Avl_string_base const &node) {
			functor(policy(node));
		});
	}

	void insert(Net::Policy &policy) { Avl_tree::insert(&policy.avl_member()); }
};

#endif /* _POLICY_H_ */
