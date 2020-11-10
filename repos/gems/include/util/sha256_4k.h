/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UTIL_SHA256_4K_H_
#define _UTIL_SHA256_4K_H_

/* Genode includes */
#include <base/output.h>

namespace Sha256_4k {

	struct Data { char values[4096]; };

	struct Hash {
		char values[32];

		void print(Genode::Output &out) const
		{
			using namespace Genode;

			for (char const c : values) {
				Genode::print(out, Hex(c, Hex::OMIT_PREFIX, Hex::PAD));
			}
		}
	};

	void hash(Data const &, Hash &);

} /* namespace Sha256_4k */

#endif /* _UTIL_SHA256_4K_H_ */
