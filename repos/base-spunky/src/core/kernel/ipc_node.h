/*
 * \brief  Peer of message-based synchronous inter-process communication
 * \author Martin stein
 * \date   2019-04-24
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__IPC_NODE_H_
#define _CORE__KERNEL__IPC_NODE_H_

/* Core includes */
#include <kernel/ada_object.h>

namespace Kernel {

	class Thread;
	class Ipc_node;
}

struct Kernel::Ipc_node : Ada_object<80>
{
	Ipc_node(Thread &thread);

	~Ipc_node();

	bool can_send_request();

	void send_request(Ipc_node &callee,
	                  bool      help);

	Kernel::Thread &helping_sink();

	void for_each_helper(void (*func)(Thread &));

	bool can_await_request();

	void await_request();

	void send_reply();

	void cancel_waiting();

	bool awaits_request() const;
};

#endif /* _CORE__KERNEL__IPC_NODE_H_ */
