/*
 * \brief   Backend for end points of synchronous interprocess communication
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__IPC_NODE_H_
#define _CORE__KERNEL__IPC_NODE_H_

/* Genode includes */
#include <util/fifo.h>

namespace Kernel
{
	class Thread;

	/**
	 * Backend for end points of synchronous interprocess communication
	 */
	class Ipc_node;
}

namespace Ada {

	template <Genode::uint32_t BYTES>
	struct Object
	{
		char _space[BYTES] { };
	} __attribute__ ((packed));
}

struct Kernel::Ipc_node : Ada::Object<128>
{
	/**
	 * Constructor
	 */
	Ipc_node(Thread &thread);

	~Ipc_node();

	/**
	 * Send a request and wait for the according reply
	 *
	 * \param callee    targeted IPC node
	 * \param help      wether the request implies a helping relationship
	 */
	bool can_send_request();
	void send_request(Ipc_node &callee,
	                  bool      help);

	/**
	 * Return root destination of the helping-relation tree we are in
	 */
	Thread &helping_sink();

	/**
	 * Call a given function for each helper
	 */
	void for_each_helper(void (*func)(Kernel::Thread &));

	/**
	 * Wait until a request has arrived and load it for handling
	 *
	 * \return  wether a request could be received already
	 */
	bool can_await_request();
	void await_request();

	/**
	 * Reply to last request if there's any
	 */
	void send_reply();

	/**
	 * If IPC node waits, cancel '_outbuf' to stop waiting
	 */
	void cancel_waiting();

	bool awaits_request() const;
};

#endif /* _CORE__KERNEL__IPC_NODE_H_ */
