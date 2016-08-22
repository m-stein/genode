/*
 * \brief  Label of a network interface
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>

#ifndef _INTERFACE_LABEL_H_
#define _INTERFACE_LABEL_H_

namespace Net {

	enum { MAX_INTERFACE_LABEL_SIZE = 64 };

	using Interface_label = Genode::String<MAX_INTERFACE_LABEL_SIZE>;
}

#endif /* _INTERFACE_LABEL_H_ */
