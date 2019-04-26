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

namespace Kernel { class Thread; }

namespace Ada {

	template <Genode::uint32_t SIZE>
	struct Object
	{
		struct Object_size_mismatch { };

		static constexpr Genode::uint32_t size() { return SIZE; }

		char space[SIZE] { };

	} __attribute__ ((packed));

	struct Ipc_node;

	Genode::uint32_t object_size(Ipc_node const &);

	template <typename T>
	static inline void assert_valid_object_size()
	{
		Genode::uint32_t const obj_size { object_size(*(T *)nullptr) };
		if (obj_size > T::size()) {
			Genode::raw("Error: Ada object has invalid size (", obj_size,")");
			throw typename T::Object_size_mismatch();
		}
	}
}

struct Ada::Ipc_node : Object<72>
{
	Ipc_node(Kernel::Thread &thread);

	~Ipc_node();

	bool can_send_request();

	void send_request(Ipc_node &callee,
	                  bool      help);

	Kernel::Thread &helping_sink();

	void for_each_helper(void (*func)(Kernel::Thread &));

	bool can_await_request();

	void await_request();

	void send_reply();

	void cancel_waiting();

	bool awaits_request() const;
};

#endif /* _CORE__KERNEL__IPC_NODE_H_ */
