/*
 * \brief  Implementation of non-core PD session upgrade
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <pd_session/client.h>
#include <base/env.h>

/* base-internal includes */
#include <base/internal/native_env.h>
#include <base/internal/globals.h>


void Genode::upgrade_pd_session_quota(Genode::size_t quota)
{
	internal_env().upgrade(Parent::Env::pd(),
	                       String<128>("ram_quota=", quota).string());
}
