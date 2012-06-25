/*
 * \brief  Virtual Machine Monitor
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/sleep.h>
#include <base/vm_state.h>
#include <vm_session/connection.h>

int main() {

	Genode::Vm_connection vm;
	Genode::Vm_state *state =
		(Genode::Vm_state*) Genode::env()->rm_session()->attach(vm.dataspace());

	Genode::memset((void*)state, 0, sizeof(Genode::Vm_state));

	Genode::sleep_forever();

	return 0;
}
