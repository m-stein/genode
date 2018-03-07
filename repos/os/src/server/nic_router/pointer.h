/*
 * \brief  Pointer that can be dereferenced only when valid
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		T    *_ptr;
		bool  _valid;

	public:

		struct Valid   : Genode::Exception { };
		struct Invalid : Genode::Exception { };

		Pointer() : _ptr(nullptr), _valid(false) { }

		Pointer(T &ref) : _ptr(&ref), _valid(true) { }

		T &deref() const
		{
			if (!_valid) { throw Invalid(); }
			return *_ptr;
		}

		void set(T &ptr)
		{
			if (_valid) { throw Valid(); }
			_ptr = &ptr;
			_valid = true;
		}

		void unset() { _valid = false; }
};

#endif /* _POINTER_H_ */
