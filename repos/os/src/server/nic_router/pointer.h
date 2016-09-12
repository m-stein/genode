/*
 * \brief  Pointer that can be dereferenced only when valid
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _POINTER_H_
#define _POINTER_H_

/* Genode includes */
#include <base/exception.h>

namespace Net { template <typename> class Pointer; }

template <typename T>
class Net::Pointer
{
	private:

		T *_ptr;

	public:

		struct Invalid : Genode::Exception { };

		Pointer() : _ptr(nullptr) { }

		Pointer(T &ptr) : _ptr(&ptr) { }

		T &deref() const
		{
			if (_ptr == nullptr) {
				throw Invalid(); }

			return *_ptr;
		}

		void invalidate() { _ptr = nullptr; }
};

#endif /* _POINTER_H_ */
