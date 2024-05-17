/*
 * \brief  DHCP server role of a domain
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <dhcp_server.h>
#include <interface.h>
#include <domain.h>
#include <configuration.h>
#include <xml_node.h>

using namespace Net;
using namespace Genode;


/**********************
 ** Dhcp_server_base **
 **********************/

Dhcp_server_base::Dhcp_server_base(Xml_node const &node,
                                   Domain   const &domain,
                                   Allocator      &alloc)
:
	_alloc { alloc }
{
	node.for_each_sub_node("dns-server", [&] (Xml_node const &sub_node) {

		Dns_server::construct(
			alloc, sub_node.attribute_value("ip", Ipv4_address { }),
			[&] /* handle_success */ (Dns_server &server)
			{
				_dns_servers.insert_as_tail(server);
			},
			[&] /* handle_failure */ ()
			{
				_invalid(domain, "invalid DNS server entry");
			}
		);
	});
	node.with_optional_sub_node("dns-domain", [&] (Xml_node const &sub_node) {
		xml_node_with_attribute(sub_node, "name", [&] (Xml_attribute const &attr) {
			_dns_domain_name.set_to(attr);

			if (domain.config().verbose() &&
			    !_dns_domain_name.valid()) {

				log("[", domain, "] rejecting oversized DNS "
				    "domain name from DHCP server configuration");
			}
		});
	});
}


Dhcp_server_base::~Dhcp_server_base()
{
	_dns_servers.destroy_each(_alloc);
}


void Dhcp_server_base::_invalid(Domain const &domain,
                                char   const *reason)
{
	if (domain.config().verbose()) {
		log("[", domain, "] invalid DHCP server (", reason, ")"); }

	throw Domain::Invalid();
}


/*****************
 ** Dhcp_server **
 *****************/

bool Dhcp_server::dns_servers_empty() const
{
	if (_dns_config_from_ptr)
		return _resolve_dns_config_from().dns_servers_empty();
	return _dns_servers.empty();
}


Dhcp_server::Dhcp_server(Xml_node            const  node,
                         Domain                    &domain,
                         Allocator                 &alloc,
                         Ipv4_address_prefix const &interface,
                         Domain_dict               &domains)
:
	Dhcp_server_base(node, domain, alloc),
	_dns_config_from_ptr(_init_dns_config_from(node, domains)),
	_ip_lease_time  (_init_ip_lease_time(node)),
	_ip_first(node.attribute_value("ip_first", Ipv4_address())),
	_ip_last(node.attribute_value("ip_last", Ipv4_address())),
	_ip_first_raw(_ip_first.to_uint32_little_endian()),
	_ip_count(_ip_last.to_uint32_little_endian() - _ip_first_raw + 1),
	_ip_alloc(alloc, _ip_count)
{
	if (!interface.prefix_matches(_ip_first)) {
		_invalid(domain, "first IP does not match domain subnet"); }

	if (!interface.prefix_matches(_ip_last)) {
		_invalid(domain, "last IP does not match domain subnet"); }

	if (interface.address.is_in_range(_ip_first, _ip_last)) {
		_invalid(domain, "IP range contains IP address of domain"); }
}


Microseconds Dhcp_server::_init_ip_lease_time(Xml_node const node)
{
	uint64_t ip_lease_time_sec =
		node.attribute_value("ip_lease_time_sec", (uint64_t)0);

	if (!ip_lease_time_sec) {
		ip_lease_time_sec = DEFAULT_IP_LEASE_TIME_SEC;
	}
	return Microseconds((uint64_t)ip_lease_time_sec * 1000 * 1000);
}


void Dhcp_server::print(Output &output) const
{
	_dns_servers.for_each([&] (Dns_server const &dns_server) {
		Genode::print(output, "DNS server ", dns_server.ip(), ", ");
	});
	_dns_domain_name.with_string([&] (Dns_domain_name::String const &str) {
		Genode::print(output, "DNS domain name ", str, ", ");
	});
	with_dns_config_from([&] (Domain &domain) {
		Genode::print(output, "DNS config from ", domain, ", "); });

	Genode::print(output, "IP first ", _ip_first,
	                        ", last ", _ip_last,
	                       ", count ", _ip_count,
	                  ", lease time ", _ip_lease_time.value / 1000 / 1000, " sec");
}


bool Dhcp_server::config_equal_to_that_of(Dhcp_server const &other) const
{
	return _ip_lease_time.value == other._ip_lease_time.value &&
	       _dns_servers.equal_to(other._dns_servers)          &&
	       _dns_domain_name.equal_to(other._dns_domain_name);
}


Ipv4_config const &Dhcp_server::_resolve_dns_config_from() const
{
	return _dns_config_from_ptr->ip_config();
}


bool Dhcp_server::alloc_any_ip(Ipv4_address &ip)
{
	addr_t ip_raw { };
	if (!_ip_alloc.alloc_any_range(ip_raw))
		return false;

	ip = Ipv4_address::from_uint32_little_endian(ip_raw + _ip_first_raw);
	return true;
}


bool Dhcp_server::alloc_given_ip(Ipv4_address const &ip)
{
	return _ip_alloc.alloc_given_range(ip.to_uint32_little_endian() - _ip_first_raw);
}


void Dhcp_server::free_ip(Domain       const &domain,
                          Ipv4_address const &ip)
{
	if (!_ip_alloc.free(ip.to_uint32_little_endian() - _ip_first_raw))
		error("[", domain, "] DHCP server: failed to free IP");
}


Domain *Dhcp_server::_init_dns_config_from(Genode::Xml_node const node, Domain_dict &domains)
{
	if (!_dns_servers.empty() ||
	    _dns_domain_name.valid()) {

		return nullptr;
	}
	Domain_name dns_config_from =
		node.attribute_value("dns_config_from", Domain_name());

	if (dns_config_from == Domain_name()) {
		return nullptr;
	}
	return &domains.deprecated_find_by_name<Invalid>(dns_config_from);
}


bool Dhcp_server::has_invalid_remote_dns_cfg() const
{
	if (_dns_config_from_ptr)
		return !_dns_config_from_ptr->ip_config().valid();
	return false;
}


/*********************
 ** Dhcp_allocation **
 *********************/

Dhcp_allocation::Dhcp_allocation(Interface      &interface,
                             Ipv4_address const &ip,
                             Mac_address  const &mac,
                             Cached_timer       &timer,
                             Microseconds        lifetime)
:
	_interface(interface), _ip(ip), _mac(mac),
	_timeout(timer, *this, &Dhcp_allocation::_handle_timeout)
{
	_interface.dhcp_stats().alive++;
	_timeout.schedule(lifetime);
}


Dhcp_allocation::~Dhcp_allocation()
{
	_interface.dhcp_stats().alive--;
	_interface.dhcp_stats().destroyed++;
}


void Dhcp_allocation::lifetime(Microseconds lifetime)
{
	_timeout.schedule(lifetime);
}


bool Dhcp_allocation::_higher(Mac_address const &mac) const
{
	return memcmp(mac.addr, _mac.addr, sizeof(_mac.addr)) > 0;
}


void Dhcp_allocation::print(Output &output) const
{
	Genode::print(output, "MAC ", _mac, " IP ", _ip);
}


void Dhcp_allocation::_handle_timeout(Duration)
{
	_interface.dhcp_allocation_expired(*this);
}
