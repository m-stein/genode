/*
 * \brief  Downlink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/session_policy.h>

/* local includes */
#include <component.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


/**************************
 ** Communication_buffer **
 **************************/

Communication_buffer::Communication_buffer(Ram_session          &ram,
                                           Genode::size_t const  size)
:
	Ram_dataspace_capability(ram.alloc(size)), _ram(ram)
{ }


/****************************
 ** Session_component_base **
 ****************************/

Session_component_base::
Session_component_base(Allocator    &guarded_alloc_backing,
                       size_t const  guarded_alloc_amount,
                       Ram_session  &buf_ram,
                       size_t const  tx_buf_size,
                       size_t const  rx_buf_size)
:
	_guarded_alloc(&guarded_alloc_backing, guarded_alloc_amount),
	_range_alloc(&_guarded_alloc), _tx_buf(buf_ram, tx_buf_size),
	_rx_buf(buf_ram, rx_buf_size)
{ }


/***********************
 ** Session_component **
 ***********************/

Net::Session_component::Session_component(Allocator           &alloc,
                                          Timer::Connection   &timer,
                                          size_t        const  amount,
                                          Ram_session         &buf_ram,
                                          size_t        const  tx_buf_size,
                                          size_t        const  rx_buf_size,
                                          Region_map          &region_map,
                                          Mac_address   const  mac,
                                          Entrypoint          &ep,
                                          Mac_address   const &router_mac,
                                          Session_label const  label,
                                          Configuration       &config)
:
	Session_component_base(alloc, amount, buf_ram, tx_buf_size, rx_buf_size),
	Session_rpc_object(region_map, _tx_buf, _rx_buf, &_range_alloc, ep.rpc_ep()),
	Interface(ep, timer, router_mac, _guarded_alloc, mac, config)
{
	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);

	/* try to find matching domain and attach interface to it */
	try {
		Session_policy policy(label, config.node());
		Domain_name domain_name(policy.attribute_value("domain", Domain_name()));
		Domain &domain = config.domains().find_by_name(domain_name);
		Interface::attach_to_domain(domain);
	}
	catch (Session_policy::No_policy_defined) { if (config.verbose()) log("No matching policy"); }
	catch (Domain_tree::No_match)             { if (config.verbose()) log("No matching domain"); }
	catch (Xml_node::Nonexistent_attribute)   { if (config.verbose()) log("No domain attribute in policy"); }
}


/**********
 ** Root **
 **********/

Net::Root::Root(Entrypoint        &ep,
                Timer::Connection &timer,
                Allocator         &alloc,
                Mac_address const &router_mac,
                Configuration     &config,
                Ram_session       &buf_ram,
                Region_map        &region_map)
:
	Root_component<Session_component>(&ep.rpc_ep(), &alloc), _timer(timer),
	_ep(ep), _router_mac(router_mac), _config(config), _buf_ram(buf_ram),
	_region_map(region_map)
{ }


Session_component *Net::Root::_create_session(char const *args)
{
	try {
		size_t const ram_quota =
			Arg_string::find_arg(args, "ram_quota").ulong_value(0);

		size_t const tx_buf_size =
			Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

		size_t const rx_buf_size =
			Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

		size_t const session_size =
			max((size_t)4096, sizeof(Session_component));

		if (ram_quota < session_size) {
			throw Insufficient_ram_quota(); }

		if (tx_buf_size               > ram_quota - session_size ||
		    rx_buf_size               > ram_quota - session_size ||
		    tx_buf_size + rx_buf_size > ram_quota - session_size)
		{
			error("insufficient 'ram_quota' for session creation");
			throw Insufficient_ram_quota();
		}
		return new (md_alloc())
			Session_component(*md_alloc(), _timer, ram_quota - session_size,
			                  _buf_ram, tx_buf_size, rx_buf_size, _region_map,
			                  _mac_alloc.alloc(), _ep, _router_mac,
			                  label_from_args(args), _config);
	}
	catch (Mac_allocator::Alloc_failed) {
		error("failed to allocate MAC address");
	}
	throw Service_denied();
}
