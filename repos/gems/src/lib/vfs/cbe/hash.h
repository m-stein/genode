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

#ifndef _TRESOR__HASH_H_
#define _TRESOR__HASH_H_

namespace Cbe {

	class Block_data;
	class Hash;
}

namespace Tresor {

	void calc_hash(Cbe::Block_data const &, Cbe::Hash &);

	bool check_hash(Cbe::Block_data const &, Cbe::Hash const &);

	Cbe::Hash hash(Cbe::Block_data const &);
}

#endif /* _TRESOR__HASH_H_ */
