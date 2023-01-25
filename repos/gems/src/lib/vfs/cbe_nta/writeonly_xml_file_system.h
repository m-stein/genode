/*
 * \brief  File system for providing a value as a file
 * \author Martin Stein
 * \date   2023-01-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VALUE_FILE_SYSTEM_H_
#define _VALUE_FILE_SYSTEM_H_

/* base includes */
#include <util/xml_generator.h>
#include <base/registry.h>

/* os includes */
#include <vfs/single_file_system.h>

namespace Vfs {

	class Writeonly_xml_file_system;
}


class Vfs::Writeonly_xml_file_system : public Vfs::Single_file_system
{
	public:

		typedef Genode::String<64> Name;

		class Content_consumer : public Genode::Interface
		{
			public:

				virtual void consume_content(char const     *buf_ptr,
				                             Genode::size_t  buf_size) = 0;
		};

		class Xml_consumer : public Content_consumer
		{
			private:

				void consume_content(char const     *buf_ptr,
				                     Genode::size_t  buf_size) override;

			public:

				Xml_consumer();

				virtual void consume_xml(Genode::Xml_node const &node) = 0;
		};

	private:

		typedef Genode::String<1024 + 1> Buffer;

		Name const    _file_name;
		Xml_consumer &_xml_consumer;
		Buffer        _buffer { };

		struct Vfs_handle : Single_vfs_handle
		{
			Writeonly_xml_file_system &_value_fs;
			Buffer                    &_buffer{ _value_fs._buffer };

			Vfs_handle(Writeonly_xml_file_system &value_fs,
			           Allocator         &alloc)
			:
				Single_vfs_handle(value_fs, value_fs, alloc, 0),
				_value_fs(value_fs)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				Genode::error("Heyho");
				out_count = 0;

				if (seek() > _buffer.length())
					return READ_ERR_INVALID;

				char const *   const src = _buffer.string() + seek();
				Genode::size_t const len = min((size_t)(_buffer.length() - seek()), (size_t)count);
				Genode::memcpy(dst, src, len);

				out_count = len;
				return READ_OK;
			}

			Write_result write(char const *src,
			                   file_size   count,
			                   file_size  &out_count) override
			{
				out_count = 0;
				try {
					_value_fs._xml_consumer.consume_xml(
						Xml_node { src, count });
				}
				catch (...) {
					return WRITE_ERR_INVALID;
				}
				out_count = count;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }

			private:

			Vfs_handle(Vfs_handle const &);
			Vfs_handle &operator = (Vfs_handle const &);
		};

		struct Watch_handle;
		using Watch_handle_registry = Genode::Registry<Watch_handle>;

		struct Watch_handle : Vfs_watch_handle
		{
			typename Watch_handle_registry::Element elem;

			Watch_handle(Watch_handle_registry &registry,
			             Vfs::File_system      &fs,
			             Allocator             &alloc)
			: Vfs_watch_handle(fs, alloc), elem(registry, *this) { }
		};

		Watch_handle_registry _watch_handle_registry { };


		void _watch_response() {
			_watch_handle_registry.for_each([&] (Watch_handle &h) {
				h.watch_response();
			});
		}

		static char const *_static_type() { return "value"; }

		typedef Genode::String<200> Config;
		Config _config(Name const &name) const
		{
			char buf[Config::capacity()] { };
			Genode::Xml_generator xml(buf, sizeof(buf), _static_type(), [&] () {
				xml.attribute("name", name); });
			return Config(Genode::Cstring(buf));
		}


	public:

		Writeonly_xml_file_system(Name   const &name,
		                          Xml_consumer &xml_consumer,
		                          Buffer const &initial_value)
		:
			Single_file_system { Node_type::TRANSACTIONAL_FILE, type(),
			                     Node_rwx::rw(),
			                     Xml_node { _config(name).string() } },
			_file_name         { name },
			_xml_consumer      { xml_consumer }
		{
			value(initial_value);
		}

		char const *type() override { return _static_type(); }

		void value(Buffer const &value)
		{
			_buffer = Buffer(value);
		}

		Genode::uint64_t value()
		{
			Genode::uint64_t val { 0 };
			Genode::ascii_to(_buffer.string(), val);

			return val;
		}

		Buffer buffer() const  { return _buffer; }

		bool matches(Xml_node node) const
		{
			return node.has_type(_static_type()) &&
			       node.attribute_value("name", Name()) == _file_name;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size size) override
		{
			if (size >= 1024)
				return FTRUNCATE_ERR_NO_SPACE;

			return FTRUNCATE_OK;
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc) Vfs_handle(*this, alloc);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { Genode::error("out of ram"); return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { Genode::error("out of caps");return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = _buffer.length();
			return result;
		}

		Watch_result watch(char const      *path,
		                   Vfs_watch_handle **handle,
		                   Allocator        &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				Watch_handle *wh = new (alloc)
					Watch_handle(_watch_handle_registry, *this, alloc);
				*handle = wh;
				return WATCH_OK;
			}
			catch (Genode::Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			if (handle && (&handle->fs() == this))
				destroy(handle->alloc(), handle);
		}
};

#endif /* _VALUE_FILE_SYSTEM_H_ */
