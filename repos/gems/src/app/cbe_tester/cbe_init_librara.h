/*
 * \brief  Temporary module compliant wrapper for the CBE library
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_INIT_LIBRARA_H_
#define _CBE_INIT_LIBRARA_H_

/* gems includes */
#include <cbe/init/library.h>

/* cbe tester includes */
#include <module.h>

namespace Cbe_init {

	using Cbe::Module;
	using Cbe::Module_request;

	class Librara;
}


class Cbe_init::Librara : public Module
{
	private:

		Cbe_init::Library &_lib;


		/************
		 ** Module **
		 ************/

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			return _lib.librara__peek_generated_request(buf_ptr, buf_size);
		}

		void _drop_generated_request(Module_request &) override;

		void generated_request_complete(Module_request &) override;

	public:

		Librara(Cbe_init::Library &lib)
		:
			_lib { lib }
		{ }
};

#endif /* _CBE_INIT_LIBRARA_H_ */
