/*
 * \brief  Object pointer that is safe against null dereferencing
 * \author Martin Stein
 * \date   2021-04-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KERNEL__POINTER_H_
#define _KERNEL__POINTER_H_

namespace Kernel {

	template <typename OBJECT_TYPE>
	class Pointer;
}


template <typename OBJECT_TYPE>
class Kernel::Pointer
{
	private:

		OBJECT_TYPE *_object;

	public:

		class Invalid : Genode::Exception { };

		Pointer() : _object { nullptr } { }

		Pointer(OBJECT_TYPE &object) : _object { &object } { }

		OBJECT_TYPE &object()
		{
			if (_object == nullptr)
				throw Invalid();

			return *_object;
		}

		bool valid() const { return _object != nullptr; }

		bool equals(Pointer const &other) const
		{
			if (valid() != other.valid()) {
				return false;
			}
			if (!valid()) {
				return true;
			}
			return _object == other._object;
		}
};

#endif /* _KERNEL__POINTER_H_ */
