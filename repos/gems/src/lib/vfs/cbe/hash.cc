/*
 * \brief  Calculate and check hashes of tresor data blocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <hash.h>
#include <cbe/types.h>
#include <util/string.h>

/* libcrypto */
#include <openssl/sha.h>

bool Tresor::check_hash(Cbe::Block_data const &blk, Cbe::Hash const &expected_hash)
{
	Cbe::Hash got_hash;
	calc_hash(blk, got_hash);
	return !Genode::memcmp(&got_hash, &expected_hash, sizeof(Cbe::Hash));
}


void Tresor::calc_hash(Cbe::Block_data const &blk, Cbe::Hash &hash)
{
	SHA256_CTX context { };
	ASSERT(SHA256_Init(&context));
	ASSERT(SHA256_Update(&context, &blk, Cbe::BLOCK_SIZE));
	ASSERT(SHA256_Final((unsigned char *)(&hash), &context));
}


Cbe::Hash Tresor::hash(Cbe::Block_data const &blk)
{
	Cbe::Hash hash { };
	calc_hash(blk, hash);
	return hash;
}
