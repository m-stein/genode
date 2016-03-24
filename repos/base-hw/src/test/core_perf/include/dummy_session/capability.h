/*
 * \brief  Dummy-session capability type
 * \author Martin Stein
 * \date   2016-03-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DUMMY_SESSION__CAPABILITY_H_
#define _DUMMY_SESSION__CAPABILITY_H_

/* Genode includes */
#include <base/capability.h>

/* local includes */
#include <dummy_session/dummy_session.h>

namespace Dummy
{
	using Genode::Capability;

	typedef Capability<Session> Session_capability;
}

#endif /* _DUMMY_SESSION__CAPABILITY_H_ */
