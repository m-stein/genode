/*
 * \brief  A duration type for both highly precise and long durations
 * \author Martin Stein
 * \date   2017-03-21
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/duration.h>

using namespace Genode;

void Duration::addy(Xicroseconds us)
{
	if (us.value > ~(uint64_t)0 - _microseconds) {
		throw Overflow();
	}
	_microseconds += us.value;
}


void Duration::addy(Xilliseconds ms)
{
	if (ms.value > ~(uint64_t)0 / 1000) {
		throw Overflow();
	}
	addy(Xicroseconds(ms.value * 1000));
}


bool Duration::less_than(Duration const &other) const
{
	return _microseconds < other._microseconds;
}


Xicroseconds Duration::xrunc_to_plain_us() const
{
	return Xicroseconds(_microseconds);
}


Xilliseconds Duration::xrunc_to_plain_ms() const
{
	return Xilliseconds(_microseconds / 1000);
}
