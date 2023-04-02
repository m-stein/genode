/*
 * \brief  Verbosity configuration
 * \author Martin Stein
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_VERBOSITY_H_
#define _CBE_VERBOSITY_H_

namespace Cbe {

	enum { VERBOSE_REKEYING   = 0 };
	enum { VERBOSE_VBA_ACCESS = 0 };
	enum { VERBOSE_CRYPTO     = 0 };
	enum { VERBOSE_BLOCK_IO   = 0 };
}

#endif /* _CBE_VERBOSITY_H_ */
