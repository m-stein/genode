/*
 * Copyright (C) 2019 Genode Labs GmbH, Componolit GmbH, secunet AG
 *
 * This file is part of the Consistent Block Encrypter project, which is
 * distributed under the terms of the GNU Affero General Public License
 * version 3.
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
