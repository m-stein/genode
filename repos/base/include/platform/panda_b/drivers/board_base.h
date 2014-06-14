/*
 * \brief  Base of the board driver
 * \author Martin stein
 * \date   2014-06-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__BOARD_BASE_H_
#define _DRIVERS__BOARD_BASE_H_

/* Genode includes */
#include <drivers/board_base/panda.h>

namespace Genode
{
	/**
	 * Base of the board driver
	 */
	class Board_base : public Panda::Board_base { };
}

#endif /* _DRIVERS__BOARD_BASE_H_ */
