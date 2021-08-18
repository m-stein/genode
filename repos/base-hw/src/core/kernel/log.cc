/*
 * \brief  Back end for serial output in the kernel
 * \author Martin Stein
 * \date   2021-07-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw Core includes */
#include <kernel/log.h>
#include <kernel/main.h>


void Kernel::log(char const c)
{
	enum {
		ASCII_LINE_FEED = 10,
		ASCII_CARRIAGE_RETURN = 13,
	};
	if (c == ASCII_LINE_FEED) {
		main_print_char(ASCII_CARRIAGE_RETURN);
	}
	main_print_char(c);
}
