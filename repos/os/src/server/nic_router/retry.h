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

	enum class Retry_command { RETRY, ABORT };

	template <typename EXCEPTION_1, typename EXCEPTION_2>
	void retry(auto const &attempt_fn, auto const &exception_fn)
	{
		while (1) {
			try {
				attempt_fn();
				return;
			}
			catch (EXCEPTION_1) { }
			catch (EXCEPTION_2) { }
			switch (exception_fn()) {
			case Retry_command::RETRY: break;
			case Retry_command::ABORT: return;
			}
		}
	}
}

#endif /* _RETRY_H_ */
