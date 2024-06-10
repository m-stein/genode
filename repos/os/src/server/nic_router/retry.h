/*
 * \brief  Utility to execute a function repeatedly
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RETRY_H_
#define _RETRY_H_

namespace Net {

	template <typename EXCEPTION_1, typename EXCEPTION_2>
	void retry(unsigned num_attempts, auto const &attempt_fn, auto const &exception_fn, auto const &failed_fn)
	{
		while (1) {
			try {
				attempt_fn();
				return;
			}
			catch (EXCEPTION_1) { }
			catch (EXCEPTION_2) { }
			num_attempts--;
			if (num_attempts)
				exception_fn();
			else {
				failed_fn();
				return;
			}
		}
	}
}

#endif /* _RETRY_H_ */
