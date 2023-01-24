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
#include <readonly_xml_file_system.h>

using namespace Genode;
using namespace Vfs;


/********************************************
 ** Readonly_xml_file_system::Xml_producer **
 ********************************************/

void
Readonly_xml_file_system::Xml_producer::produce_content(char   *buf_ptr,
                                                        size_t  buf_size)
{
	Xml_generator xml(buf_ptr, buf_size, _node_type.string(), [&] () {
		produce_xml(xml);
	});
}


Readonly_xml_file_system::Xml_producer::Xml_producer(Node_type node_type)
:
	_node_type { node_type }
{ }


/******************************************
 ** Readonly_xml_file_system::Vfs_handle **
 ******************************************/

Readonly_xml_file_system::
Vfs_handle::Vfs_handle(Directory_service &dir_service,
                       File_io_service   &fs,
                       Allocator         &alloc,
                       Content_producer  &content_producer)
:
	Single_vfs_handle { dir_service, fs, alloc, 0 },
	_content_producer { content_producer }
{ }


Readonly_xml_file_system::Read_result
Readonly_xml_file_system::Vfs_handle::read(char      *buf_ptr,
                                           file_size  buf_size,
                                           file_size &nr_of_read_bytes)
{
	nr_of_read_bytes = 0;
	try {
		_content_producer.produce_content(buf_ptr, buf_size);
	}
	catch (Content_producer::Buffer_capacity_exceeded) {

		error("attempt to read r/o XML file with insufficient buffer size");
		return READ_ERR_IO;
	}
	nr_of_read_bytes = buf_size;
	return READ_OK;
}


Readonly_xml_file_system::Write_result
Readonly_xml_file_system::Vfs_handle::write(char const *,
                                            file_size   ,
                                            file_size  &)
{
	return WRITE_ERR_IO;
}


/******************************
 ** Readonly_xml_file_system **
 ******************************/

Readonly_xml_file_system::
Readonly_xml_file_system(File_name const  &file_name,
                         Content_producer &content_producer)
:
	Single_file_system { Node_type::TRANSACTIONAL_FILE, type(),
	                     Node_rwx::ro(),
	                     Xml_node { _config(file_name).string() } },
	_file_name         { file_name },
	_content_producer  { content_producer }
{ }


Readonly_xml_file_system::Open_result
Readonly_xml_file_system::open(char const       *path,
                               unsigned          /* mode */,
                               Vfs::Vfs_handle **out_handle,
                               Allocator        &alloc)
{
	if (!_single_file(path))
		return OPEN_ERR_UNACCESSIBLE;

	try {
		*out_handle = new (alloc)
			Vfs_handle(*this, *this, alloc, _content_producer);

		return OPEN_OK;
	}
	catch (Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
	catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
}


Readonly_xml_file_system::Stat_result
Readonly_xml_file_system::stat(char const *path,
                               Stat       &out)
{
	Stat_result result { Single_file_system::stat(path, out) };
	out.size = 0;
	return result;
}


Readonly_xml_file_system::Watch_result
Readonly_xml_file_system::watch(char const        *path,
                                Vfs_watch_handle **handle,
                                Allocator         &alloc)
{
	if (!_single_file(path))
		return WATCH_ERR_UNACCESSIBLE;

	try {
		*handle = new (alloc)
			Registered_watch_handle(_handle_registry, *this, alloc);

		return WATCH_OK;
	}
	catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
	catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
}


void Readonly_xml_file_system::close(Vfs_watch_handle *handle)
{
	destroy(handle->alloc(), static_cast<Registered_watch_handle *>(handle));
}


Readonly_xml_file_system::Config
Readonly_xml_file_system::_config(File_name const &file_name) const
{
	char buf[Config::capacity()] { };

	Genode::Xml_generator xml(buf, sizeof(buf), _static_type(), [&] () {
		xml.attribute("name", file_name);
	});
	return Config { Genode::Cstring { buf } };
}
