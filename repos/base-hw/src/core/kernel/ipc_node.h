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

namespace Kernel {

	class Thread;

	/**
	 * Backend for end points of synchronous interprocess communication
	 */
	class Ipc_node;
}

class Kernel::Ipc_node
{
	private:

		using Queue_item = Genode::Fifo_element<Ipc_node>;
		using Queue      = Genode::Fifo<Queue_item>;

		enum In_state
		{
			IN_READY,
			IN_WAIT,
			IN_REPLY,
			IN_REPLY_NO_SENDER,
			IN_DESTRUCT,
		};

		enum Out_state
		{
			OUT_READY,
			OUT_SEND,
			OUT_SEND_HELPING,
			OUT_DESTRUCT,
		};

		Thread     &_thread;
		Queue_item  _queue_item { *this };
		In_state    _in_state   { IN_READY };
		Out_state   _out_state  { OUT_READY };
		Ipc_node   *_caller     { nullptr };
		Ipc_node   *_out_node   { nullptr };
		Queue       _in_queue   { };

		bool _out_sending() const
		{
			return _out_state == OUT_SEND_HELPING || _out_state == OUT_SEND;
		}

		bool _in_waiting() const
		{
			return _in_state == IN_WAIT;
		}

		/**
		 * Receive a message from another IPC node
		 */
		void _receive_from(Ipc_node &node);

		/**
		 * Cancel an ongoing send operation
		 */
		void _cancel_send();

		/**
		 * Return wether this IPC node is helping another one
		 */
		bool _helping() const;

		/**
		 * Make the class noncopyable because it has pointer members
		 */
		Ipc_node(const Ipc_node&) = delete;

		/**
		 * Make the class noncopyable because it has pointer members
		 */
		const Ipc_node& operator=(const Ipc_node&) = delete;

	public:

		/**
		 * Destructor
		 */
		~Ipc_node();

		/**
		 * Constructor
		 */
		Ipc_node(Thread &thread);

		/**
		 * Send a request and wait for the according reply
		 *
		 * \param node  targeted IPC node
		 * \param help  wether the request implies a helping relationship
		 */
		bool can_send_request() const;
		void send_request(Ipc_node &node,
		                  bool      help);

		/**
		 * Return root destination of the helping-relation tree we are in
		 */
		Thread &helping_sink();

		/**
		 * Call function 'f' of type 'void (Ipc_node *)' for each helper
		 */
		template <typename F> void for_each_helper(F f)
		{
			/* if we have a helper in the receive buffer, call 'f' for it */
			if (_caller && _caller->_helping())
				f(_caller->_thread);

			/* call 'f' for each helper in our request queue */
			_in_queue.for_each([f] (Queue_item &item) {
				Ipc_node &node { item.object() };

				if (node._helping())
					f(node._thread);
			});
		}

		/**
		 * Wait until a request has arrived and load it for handling
		 *
		 * \return  wether a request could be received already
		 */
		bool can_await_request() const;
		void await_request();

		/**
		 * Reply to last request if there's any
		 */
		void send_reply();

		/**
		 * If IPC node waits, cancel '_outbuf' to stop waiting
		 */
		void cancel_waiting();

		bool awaits_request() const { return _in_waiting(); }
};

#endif /* _CORE__KERNEL__IPC_NODE_H_ */
