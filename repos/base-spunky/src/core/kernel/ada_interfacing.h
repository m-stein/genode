/*
 * \brief  C++ utilities for interfacing Ada
 * \author Martin stein
 * \date   2019-04-24
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__ADA_INTERFACING_H_
#define _CORE__KERNEL__ADA_INTERFACING_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/log.h>

/* base-hw includes */
#include <kernel/types.h>

namespace Kernel {

	/**
	 * Base type for C++ types that emulate structured Ada types without
	 * reflecting the type layout to C++ (they are opaque to C++ code)
	 */
	template <typename CPP_TYPE, size_t CPP_SIZE>
	class Opaque_ada_type
	{
		private:

			/*
			 * Reserve sufficient space for the underlying Ada type
			 */
			char space[CPP_SIZE] { };

			size_t _ada_size() const;

		public:

			static inline void assert_types_have_same_size()
			{
				size_t const ada_size { ((CPP_TYPE *)nullptr)->_ada_size() };

				if (ada_size == CPP_SIZE) {
					return;
				}

				Genode::log(
					__PRETTY_FUNCTION__,
					": size in C++: ", CPP_SIZE,
					", size in Ada: ", ada_size);

				while (1) ;
			}

	} __attribute__ ((packed));

	/**
	 * Base type for C++ types that emulate structured Ada types while
	 * reflecting the type layout to C++ (members are also used by C++ code)
	 */
	template <typename CPP_TYPE>
	class Imitating_ada_type
	{
		private:

			size_t _ada_size() const;

		public:

			static inline void assert_types_have_same_size()
			{
				size_t const ada_size { ((CPP_TYPE *)nullptr)->_ada_size() };

				if (ada_size == sizeof(CPP_TYPE)) {
					return;
				}

				Genode::log(
					__PRETTY_FUNCTION__,
					": size in C++: ", sizeof(CPP_TYPE),
					", size in Ada: ", ada_size);

				while (1) ;
			}
	};
}

#endif /* _CORE__KERNEL__ADA_INTERFACING_H_ */
