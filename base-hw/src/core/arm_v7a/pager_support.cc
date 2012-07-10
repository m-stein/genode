/*
 * \brief   Pager parts that are specific to ARM V7A
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/pager.h>

namespace Genode
{
	bool instruction_writes_word(unsigned * const code,
	                             unsigned & value,
	                             unsigned const thread_id)
	{ return 0; }
}


void Genode::Pager_object::instruction_processed(unsigned * const code,
                                                 unsigned const read)
{ }

