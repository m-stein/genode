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


/*******************************************
 ** Writeonly_xml_file_system::Vfs_handle **
 *******************************************/

Writeonly_xml_file_system::
Vfs_handle::Vfs_handle(Writeonly_xml_file_system &fs,
                       Allocator                 &alloc)
:
	Single_vfs_handle { fs, fs, alloc, 0 },
	_fs               { fs }
{ }


Writeonly_xml_file_system::Read_result
Writeonly_xml_file_system::Vfs_handle::read(char      * /* buf_ptr */,
                                            file_size   /* buf_size */,
                                            file_size & /* nr_of_read_bytes */)
{
	return READ_ERR_INVALID;
}


Writeonly_xml_file_system::Write_result
Writeonly_xml_file_system::Vfs_handle::write(char const *buf_ptr,
                                             file_size   buf_size,
                                             file_size  &nr_of_written_bytes)
{
	nr_of_written_bytes = 0;
	try {
		_fs._xml_consumer.consume_xml(
			Xml_node { buf_ptr, buf_size });
	}
	catch (...) {
		return WRITE_ERR_INVALID;
	}
	nr_of_written_bytes = buf_size;
	return WRITE_OK;
}


/*******************************
 ** Writeonly_xml_file_system **
 *******************************/

Writeonly_xml_file_system::Config
Writeonly_xml_file_system::_config(File_name const &file_name) const
{
	char buf[Config::capacity()] { };

	Xml_generator xml { buf, sizeof(buf), _static_type(), [&] () {
		xml.attribute("name", file_name);
	} };
	return Config { Cstring(buf) };
}


Writeonly_xml_file_system::
Writeonly_xml_file_system(File_name const &file_name,
                          Xml_consumer    &xml_consumer)
:
	Single_file_system { Node_type::TRANSACTIONAL_FILE, type(), Node_rwx::rw(),
	                     Xml_node { _config(file_name).string() } },
	_file_name         { file_name },
	_xml_consumer      { xml_consumer }
{ }


Writeonly_xml_file_system::Ftruncate_result
Writeonly_xml_file_system::ftruncate(Vfs::Vfs_handle * /* handle */,
                                     file_size         /* size */)
{
	return FTRUNCATE_ERR_NO_SPACE;
}


Writeonly_xml_file_system::Open_result
Writeonly_xml_file_system::open(char const       *path,
                                unsigned          /* mode */,
                                Vfs::Vfs_handle **out_handle,
                                Allocator        &alloc)
{
	if (!_single_file(path))
		return OPEN_ERR_UNACCESSIBLE;

	try {
		*out_handle = new (alloc) Vfs_handle { *this, alloc };
		return OPEN_OK;
	}
	catch (Out_of_ram)  { error("out of ram"); return OPEN_ERR_OUT_OF_RAM; }
	catch (Out_of_caps) { error("out of caps");return OPEN_ERR_OUT_OF_CAPS; }
}


Writeonly_xml_file_system::Stat_result
Writeonly_xml_file_system::stat(char const *path,
                                Stat       &out)
{
	Stat_result result { Single_file_system::stat(path, out) };
	out.size = 0;
	return result;
}


Writeonly_xml_file_system::Watch_result
Writeonly_xml_file_system::watch(char const        * /* path */,
                                 Vfs_watch_handle ** /* handle */,
                                 Allocator         & /* alloc */)
{
	return WATCH_ERR_UNACCESSIBLE;
}
