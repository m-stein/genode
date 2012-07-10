/*
 * \brief  Assert a condition
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__ASSERT_H_
#define _INCLUDE__UTIL__ASSERT_H_

/* Genode includes */
#include <base/printf.h>

/**
 * Assert a condition
 *
 * \param  expression  expresses condition that shall be true
 */
#define assert(expression) \
	do { \
		if (!(expression)) { \
			PERR("Assertion failed: "#expression""); \
			PERR("  File: %s:%d", __FILE__, __LINE__); \
			PERR("  Function: %s", __PRETTY_FUNCTION__); \
			while (1) ; \
		} \
	} while (0) ;

#endif /* _INCLUDE__UTIL__ASSERT_H_ */

