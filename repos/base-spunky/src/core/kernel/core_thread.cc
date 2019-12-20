/*
 * \brief  Hook for preparing the first execution of core
 * \author Martin Stein
 * \date   2019-12-19
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/ada_object.h>
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>

using namespace Kernel;

void prepare_core_thread()
{
	assert_valid_ada_object_size<Ipc_node>();
	assert_valid_ada_object_size<Signal_handler>();
	assert_valid_ada_object_size<Signal_context_killer>();
	assert_valid_ada_object_size<Signal_context>();
	assert_valid_ada_object_size<Signal_receiver>();
}
