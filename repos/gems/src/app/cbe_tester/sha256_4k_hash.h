/*
 * \brief  Calculate SHA256 hash over data blocks of a size of 4096 bytes
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SHAE256_4K_HASH_H_
#define _SHAE256_4K_HASH_H_

namespace Cbe {

	void sha256_4k_hash(void *data_ptr, void *hash_ptr);
}

#endif /* _SHAE256_4K_HASH_ */
