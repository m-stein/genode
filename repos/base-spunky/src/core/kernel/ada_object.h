/*
 * \brief  C++ place-holder class for Ada objects
 * \author Martin stein
 * \date   2019-04-24
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__ADA_OBJECT_H_
#define _CORE__KERNEL__ADA_OBJECT_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/log.h>

/* base-hw includes */
#include <kernel/types.h>

namespace Kernel {

	template <size_t SIZE>
	struct Ada_object
	{
		static constexpr size_t size() { return SIZE; }

		char space[SIZE] { };

	} __attribute__ ((packed));

	struct Ipc_node;
	struct Signal_receiver;
	struct Signal_handler;
	struct Signal_context;
	struct Signal_context_killer;
	struct Cpu_share;
	struct Cpu_scheduler;
	struct Timer;
	struct Timeout;
}
namespace Board {

	struct Pic;
}
namespace Genode {

	class Cpu;
}
namespace Kernel {

	size_t object_size(Ipc_node const &);
	size_t object_size(Signal_receiver const &);
	size_t object_size(Signal_handler const &);
	size_t object_size(Signal_context const &);
	size_t object_size(Signal_context_killer const &);
	size_t object_size(Cpu_share const &);
	size_t object_size(Cpu_scheduler const &);
	size_t object_size(Timer const &);
	size_t object_size(Timeout const &);
	size_t object_size(Board::Pic const &);
	size_t object_size(Genode::Cpu const &);

	template <typename T>
	static inline void assert_valid_ada_object_size()
	{
		size_t const obj_size { object_size(*(T *)nullptr) };
		if (obj_size > T::Ada_object::size()) {
			Genode::error("Ada object has invalid size (should be ", obj_size,")");
			while (1) ;
		}
	}
}

#endif /* _CORE__KERNEL__ADA_OBJECT_H_ */
