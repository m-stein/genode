/*
 * \brief  Reflects an effective domain configuration node
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DOMAIN_H_
#define _DOMAIN_H_

/* local includes */
#include <forward_rule.h>
#include <transport_rule.h>
#include <nat_rule.h>
#include <ip_rule.h>
#include <port_allocator.h>
#include <pointer.h>
#include <bit_allocator_dynamic.h>

/* Genode includes */
#include <util/avl_string.h>
#include <util/xml_node.h>
#include <util/noncopyable.h>
#include <os/duration.h>

namespace Genode { class Allocator; }

namespace Net {

	class Interface;
	class Configuration;
	class Dhcp_server;
	class Domain_avl_member;
	class Domain_base;
	class Domain;
	class Domain_tree;
	using Domain_name = Genode::String<160>;
}


class Net::Dhcp_server : Genode::Noncopyable
{
	private:

		Ipv4_address         const    _dns_server;
		Genode::Microseconds const    _ip_lease_time;
		Ipv4_address         const    _ip_first;
		Ipv4_address         const    _ip_last;
		Genode::uint32_t     const    _ip_first_raw;
		Genode::uint32_t     const    _ip_count;
		Genode::Bit_allocator_dynamic _ip_alloc;

		Genode::Microseconds _init_ip_lease_time(Genode::Xml_node const node);

	public:

		enum { DEFAULT_IP_LEASE_TIME_SEC = 3600 };

		struct Alloc_ip_failed : Genode::Exception { };
		struct Invalid         : Genode::Exception { };

		Dhcp_server(Genode::Xml_node    const  node,
		            Genode::Allocator         &alloc,
                    Ipv4_address_prefix const &interface);

		Ipv4_address alloc_ip();

		void free_ip(Ipv4_address const &ip);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address   const &dns_server()    const { return _dns_server; }
		Genode::Microseconds  ip_lease_time() const { return _ip_lease_time; }
};


class Net::Domain_avl_member : public Genode::Avl_string_base
{
	private:

		Domain &_domain;

	public:

		Domain_avl_member(Domain_name const &name,
		                  Domain            &domain);


		/***************
		 ** Accessors **
		 ***************/

		Domain &domain() const { return _domain; }
};


class Net::Domain_base
{
	protected:

		Domain_name const _name;

		Domain_base(Genode::Xml_node const node);
};


class Net::Domain : public Domain_base
{
	private:

		Domain_avl_member     _avl_member;
		Configuration        &_config;
		Genode::Xml_node      _node;
		Genode::Allocator    &_alloc;
		Ipv4_address_prefix   _interface_attr;
		bool                  _interface_attr_valid;
		Ipv4_address const    _gateway;
		bool         const    _gateway_valid;
		Ip_rule_list          _ip_rules;
		Forward_rule_tree     _tcp_forward_rules;
		Forward_rule_tree     _udp_forward_rules;
		Transport_rule_list   _tcp_rules;
		Transport_rule_list   _udp_rules;
		Port_allocator        _tcp_port_alloc;
		Port_allocator        _udp_port_alloc;
		Nat_rule_tree         _nat_rules;
		Pointer<Interface>    _interface;
		Pointer<Dhcp_server>  _dhcp_server;

		void _read_forward_rules(Genode::Cstring  const &protocol,
		                         Domain_tree            &domains,
		                         Genode::Xml_node const  node,
		                         char             const *type,
		                         Forward_rule_tree      &rules);

		void _read_transport_rules(Genode::Cstring  const &protocol,
		                           Domain_tree            &domains,
		                           Genode::Xml_node const  node,
		                           char             const *type,
		                           Transport_rule_list    &rules);

		void _interface_attr_became_valid();

	public:

		struct Invalid     : Genode::Exception { };
		struct No_next_hop : Genode::Exception { };

		Domain(Configuration          &config,
		       Genode::Xml_node const  node,
		       Genode::Allocator      &alloc);

		~Domain();

		void create_rules(Domain_tree &domains);

		Ipv4_address const &next_hop(Ipv4_address const &ip) const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		bool                 gateway_valid()        const { return _gateway_valid; }
		bool                 interface_attr_valid() const { return _interface_attr_valid; }
		Domain_name const   &name()                       { return _name; }
		Ip_rule_list        &ip_rules()                   { return _ip_rules; }
		Forward_rule_tree   &tcp_forward_rules()          { return _tcp_forward_rules; }
		Forward_rule_tree   &udp_forward_rules()          { return _udp_forward_rules; }
		Transport_rule_list &tcp_rules()                  { return _tcp_rules; }
		Transport_rule_list &udp_rules()                  { return _udp_rules; }
		Nat_rule_tree       &nat_rules()                  { return _nat_rules; }
		Ipv4_address_prefix &interface_attr()             { return _interface_attr; };
		Pointer<Interface>  &interface()                  { return _interface; }
		Configuration       &config()               const { return _config; }
		Domain_avl_member   &avl_member()                 { return _avl_member; }
		Dhcp_server         &dhcp_server()                { return _dhcp_server.deref(); }
};


struct Net::Domain_tree : Genode::Avl_tree<Genode::Avl_string_base>
{
	using Avl_tree = Genode::Avl_tree<Genode::Avl_string_base>;

	struct No_match : Genode::Exception { };

	static Domain &domain(Genode::Avl_string_base const &node);

	Domain &find_by_name(Domain_name name);

	template <typename FUNC>
	void for_each(FUNC && functor) const {
		Avl_tree::for_each([&] (Genode::Avl_string_base const &node) {
			functor(domain(node));
		});
	}

	void insert(Domain &domain) { Avl_tree::insert(&domain.avl_member()); }
};

#endif /* _DOMAIN_H_ */
