/*
 * \brief  Port routing entry
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
#include <port_route.h>

using namespace Net;
using namespace Genode;


Port_route::Port_route(uint16_t nr, char const *label, size_t label_size,
                       Ipv4_address via, Ipv4_address to)
:
	_nr(nr), _label(label, label_size), _via(via), _to(to)
{ }


void Port_route::print(Output &output) const
{
	Genode::print(output, "", _nr, " -> \"", _label, "\" to ", _to,
	              " via ", _via);
}


Port_route *Port_route::find_by_nr(uint16_t nr)
{
	if (nr == _nr) {
		return this; }

	bool const side = nr > _nr;
	Port_route *c = Avl_node<Port_route>::child(side);
	return c ? c->find_by_nr(nr) : 0;
}


Port_route *Port_route_tree::find_by_nr(uint16_t nr)
{
	Port_route *port = first();
	if (!port) {
		return port; }

	port = port->find_by_nr(nr);
	return port;
}
