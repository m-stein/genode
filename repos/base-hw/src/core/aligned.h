/*
 * \brief   Creating aligned data members in classes with unknown alignment
 * \author  Martin Stein
 * \date    2021-07-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KERNEL__ALIGNED_H_
#define _KERNEL__ALIGNED_H_

/* Genode includes */
#include <util/construct_at.h>

namespace Genode {

	template <typename    OBJ_TYPE,
	          unsigned    OBJ_ALIGNM_IN_BYTES_LOG2,
	          typename... OBJ_CONSTR_ARGS>
	class Aligned;
}


template <typename    OBJ_TYPE,
          unsigned    OBJ_ALIGNM_IN_BYTES_LOG2,
          typename... OBJ_CONSTR_ARGS>
class Genode::Aligned {

	private:

		uint8_t   _obj_space[sizeof(OBJ_TYPE) + (1 << OBJ_ALIGNM_IN_BYTES_LOG2) - 1];
		OBJ_TYPE &_obj;

		OBJ_TYPE &_initialize_obj(OBJ_CONSTR_ARGS &&... obj_constr_args)
		{
			addr_t const obj_addr {
				align_addr(
					(addr_t)_obj_space, (int)OBJ_ALIGNM_IN_BYTES_LOG2) };

			return
				*construct_at<OBJ_TYPE>((void *)obj_addr, obj_constr_args...);
		}

	public:

		Aligned(OBJ_CONSTR_ARGS &&... obj_constr_args)
		:
			_obj { _initialize_obj(obj_constr_args...) }
		{ }

		OBJ_TYPE &obj() { return _obj; }
};

#endif /* _KERNEL__ALIGNED_H_ */
