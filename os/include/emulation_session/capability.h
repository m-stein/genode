/*
 * \brief   Capability of an emulation session
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__EMULATION_SESSION__CAPABILITY_H_
#define _INCLUDE__EMULATION_SESSION__CAPABILITY_H_

/* Genode includes */
#include <base/capability.h>

namespace Emulation
{
	using namespace Genode;

	class Session;

	/**
	 * Capability of an emulation session
	 */
	typedef Capability<Session> Session_capability;
}

#endif /* _INCLUDE__EMULATION_SESSION__CAPABILITY_H_ */

