/*
 * \brief  Reflects the current router configuration through objects
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

/* local includes */
#include <policy.h>

namespace Genode { class Allocator; }

namespace Net { class Configuration; }


class Net::Configuration
{
	private:

		Genode::Allocator &_alloc;
		bool     const     _verbose;
		unsigned const     _rtt_sec;
		Policy_tree        _policies;

	public:

		Configuration(Genode::Xml_node const node, Genode::Allocator &alloc);


		/***************
		 ** Accessors **
		 ***************/

		bool         verbose() const { return _verbose; }
		unsigned     rtt_sec() const { return _rtt_sec; }
		Policy_tree &policies()      { return _policies; }
};

#endif /* _CONFIGURATION_H_ */
