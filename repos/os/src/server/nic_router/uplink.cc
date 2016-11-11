/*
 * \brief  Uplink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <net/ethernet.h>

/* local includes */
#include <uplink.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


Net::Uplink::Uplink(Entrypoint        &ep,
                    Genode::Timer     &timer,
                    Genode::Allocator &alloc,
                    Configuration     &config)
:
	Nic::Packet_allocator(&alloc),
	Nic::Connection(this, BUF_SIZE, BUF_SIZE),
	Interface(ep, timer, mac_address(), alloc, Mac_address(),
	          config.policies().find_by_label(Cstring("uplink")))
{
	rx_channel()->sigh_ready_to_ack(_sink_ack);
	rx_channel()->sigh_packet_avail(_sink_submit);
	tx_channel()->sigh_ack_avail(_source_ack);
	tx_channel()->sigh_ready_to_submit(_source_submit);
}
