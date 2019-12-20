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

namespace Kernel {

	using Ada_object_size = Genode::uint32_t;

	template <Ada_object_size SIZE>
	struct Ada_object
	{
		struct Size_mismatch { };

		static constexpr Ada_object_size size() { return SIZE; }

		char space[SIZE] { };

	} __attribute__ ((packed));

	struct Ipc_node;
	struct Signal_receiver;
	struct Signal_handler;
	struct Signal_context;
	struct Signal_context_killer;

	Ada_object_size object_size(Ipc_node const &);
	Ada_object_size object_size(Signal_receiver const &);
	Ada_object_size object_size(Signal_handler const &);
	Ada_object_size object_size(Signal_context const &);
	Ada_object_size object_size(Signal_context_killer const &);

	template <typename T>
	static inline void assert_valid_ada_object_size()
	{
		Ada_object_size const obj_size { object_size(*(T *)nullptr) };
		if (obj_size > T::Ada_object::size()) {
			Genode::error("Ada object has invalid size (should be ", obj_size,")");
			throw typename T::Ada_object::Size_mismatch();
		}
	}

	void assert_valid_ada_object_sizes();
}

#endif /* _CORE__KERNEL__ADA_OBJECT_H_ */
