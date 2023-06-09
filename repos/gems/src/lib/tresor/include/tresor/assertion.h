/*
 * \brief  Assertion macros
 * \author Martin Stein
 * \date   2023-06-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__ASSERTION_H_
#define _TRESOR__ASSERTION_H_

/* base includes */
#include <base/log.h>

#define ASSERT(condition) \
	do \
	{ \
		if (!(condition)) { \
			Genode::error(__FILE__, ":", __LINE__, ": ", " assertion \"", #condition, "\" failed "); \
			for (;;); \
		} \
	} while(0)

#define ASSERT_NEVER_REACHED \
	do { \
		Genode::error(__FILE__, ":", __LINE__, ": ", " should have never been reached"); \
		for (;;); \
	} while (false)

#endif /* _TRESOR__ASSERTION_H_ */
