/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2014-07-14
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__UNIPROCESSOR__CPU_SUPPORT_H_
#define _SPEC__UNIPROCESSOR__CPU_SUPPORT_H_

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Uniprocessor;
}

class Genode::Uniprocessor
{
	public:

		static unsigned primary_id() { return 0; }
		static unsigned executing_id() { return primary_id(); }
};

#endif /* _SPEC__UNIPROCESSOR__CPU_SUPPORT_H_ */
