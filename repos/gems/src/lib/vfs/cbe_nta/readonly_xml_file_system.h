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

#ifndef _READONLY_XML_FILE_SYSTEM_H_
#define _READONLY_XML_FILE_SYSTEM_H_

/* base includes */
#include <util/xml_generator.h>
#include <base/registry.h>

/* local includes */
#include <vfs/single_file_system.h>

namespace Vfs {

	class Readonly_xml_file_system;
}


class Vfs::Readonly_xml_file_system : public Vfs::Single_file_system
{
	public:

		using File_name               = Genode::String<64> ;
		using Config                  = Genode::String<200> ;
		using Registered_watch_handle = Genode::Registered<Vfs_watch_handle>;
		using Watch_handle_registry   =
			Genode::Registry<Registered_watch_handle>;

		class Content_producer : public Genode::Interface
		{
			public:

				using Buffer_capacity_exceeded =
					Genode::Xml_generator::Buffer_exceeded;

				virtual void produce_content(char           *buf_ptr,
				                             Genode::size_t  buf_size) = 0;
		};

		class Xml_producer : public Content_producer
		{
			public:

				typedef Genode::String<64> Node_type;

			private:

				Node_type const _node_type;

				void produce_content(char           *buf_ptr,
				                     Genode::size_t  buf_size) override;

			public:

				Xml_producer(Node_type node_type);

				virtual void produce_xml(Genode::Xml_generator &) = 0;
		};

	private:

		class Vfs_handle : public Single_vfs_handle
		{
			private:

				Content_producer &_content_producer;

			public:

				Vfs_handle(Directory_service &dir_service,
				           File_io_service   &fs,
				           Genode::Allocator &alloc,
				           Content_producer  &content_producer);

				Read_result read(char      *buf_ptr,
				                 file_size  buf_size,
				                 file_size &nr_of_read_bytes) override;

				Write_result write(char const *buf_ptr,
				                   file_size   buf_size,
				                   file_size  &nr_of_written_bytes) override;

				bool read_ready() const override { return true; }

				bool write_ready() const override { return false; }
		};

		File_name const        _file_name;
		Content_producer      &_content_producer;
		Watch_handle_registry  _handle_registry { };

		static char const *_static_type() { return "readonly_xml"; }

		Config _config(File_name const &file_name) const;

	public:

		Readonly_xml_file_system(File_name const  &file_name,
		                         Content_producer &content_producer);

		char const *type() override { return _static_type(); }

		Open_result open(char const       *path,
		                 unsigned          mode,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator        &alloc) override;

		Stat_result stat(char const *path,
		                 Stat       &out) override;

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override;

		void close(Vfs_watch_handle *handle) override;
};

#endif /* _READONLY_XML_FILE_SYSTEM_H_ */
