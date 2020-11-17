/*
 * \brief  Genode XML nodes plus local utilities
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
#include <xml_node.h>

using namespace Genode;


Microseconds Genode::read_sec_attr(Xml_node const  node,
                                   char     const *name,
                                   uint64_t const  default_sec)
{
	uint64_t sec = node.attribute_value(name, ~(uint64_t)0);
	if (sec == ~(uint64_t)0) {
		sec = default_sec;
	}
	return Microseconds(sec * 1000 * 1000);
}


Microseconds Genode::read_ms_attr(Xml_node const  node,
                                  char     const *name,
                                  uint64_t const  default_ms)
{
	uint64_t ms = node.attribute_value(name, ~(uint64_t)0);
	if (ms == ~(uint64_t)0) {
		ms = default_ms;
	}
	return Microseconds(ms * 1000);
}
