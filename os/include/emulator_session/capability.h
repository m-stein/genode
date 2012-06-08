/*
 * \brief   Capability of an emulator session
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__EMULATOR_SESSION__CAPABILITY_H_
#define _INCLUDE__EMULATOR_SESSION__CAPABILITY_H_

/* Genode includes */
#include <base/capability.h>

namespace Emulator
{
	/**
	 * Capability of an emulator session
	 */
	class Session;
	typedef Genode::Capability<Session> Session_capability;
}

#endif /* _INCLUDE__EMULATOR_SESSION__CAPABILITY_H_ */

