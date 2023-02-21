/*
 * \brief  Framework for component internal modularization
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>

using namespace Genode;
using namespace Cbe;


Module_request::Module_request(unsigned long src_module_id,
                               unsigned long src_request_id,
                               unsigned long dst_module_id)
:
	_src_module_id  { src_module_id },
	_src_request_id { src_request_id },
	_dst_module_id  { dst_module_id }
{ }


String<32> Module_request::src_request_id_str() const
{
	return
		_src_request_id == ~0UL ?
			String<32> { "?" } : String<32> { _src_request_id };
}


String<32> Module_request::dst_request_id_str() const
{
	return
		_dst_request_id == ~0UL ?
			String<32> { "?" } : String<32> { _dst_request_id };
}
