/*
 * \brief  A single read-only file that contains dynamically generated XML
 * \author Martin Stein
 * \date   2023-01-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <writeonly_xml_file_system.h>

using namespace Genode;
using namespace Vfs;


/********************************************
 ** Writeonly_xml_file_system::Xml_consumer **
 ********************************************/

void
Writeonly_xml_file_system::Xml_consumer::consume_content(char const *buf_ptr,
                                                         size_t      buf_size)
{
	Xml_node const node { buf_ptr, buf_size };
	consume_xml(node);
}


Writeonly_xml_file_system::Xml_consumer::Xml_consumer() { }
