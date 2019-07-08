/*
 * \brief  Entry point for POSIX applications
 * \author Norman Feske
 * \date   2016-12-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>

/* libc includes */
#include <stdlib.h> /* 'malloc' */
#include <stdlib.h> /* 'exit'   */

extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;

/* initial environment for the FreeBSD libc implementation */
extern char **environ;

/* provided by the application */
extern "C" int main(int argc, char ** argv, char **envp);


static void construct_component(Libc::Env &env)
{
	using Genode::Xml_node;
	using Genode::Xml_attribute;

	env.config([&] (Xml_node const &node) {
		int argc = 0;
		int envc = 0;
		char **argv;
		char **envp;

		/* count the number of arguments and environment variables */
		node.for_each_sub_node([&] (Xml_node const &node) {
			/* check if the 'value' attribute exists */
			if (node.has_type("arg") && node.has_attribute("value"))
				++argc;
			else
			if (node.has_type("env") && node.has_attribute("key") && node.has_attribute("value"))
				++envc;
		});

		if (argc == 0 && envc == 0)
			return; /* from lambda */

		/* arguments and environment are a contiguous array (but don't count on it) */
		argv = (char**)malloc((argc + envc + 1) * sizeof(char*));
		envp = &argv[argc];

		/* read the arguments */
		int arg_i = 0;
		int env_i = 0;
		node.for_each_sub_node([&] (Xml_node const &node) {

			/* insert an argument */
			if (node.has_type("arg")) {

				Xml_attribute attr = node.attribute("value");
				attr.with_raw_value([&] (char const *start, size_t length) {

					size_t const size = length + 1; /* for null termination */

					argv[arg_i] = (char *)malloc(size);

					Genode::strncpy(argv[arg_i], start, size);
				});

				++arg_i;
			}
			else

			/* insert an environment variable */
			if (node.has_type("env")) try {

				Xml_attribute const key   = node.attribute("key");
				Xml_attribute const value = node.attribute("value");

				using namespace Genode;

				/*
				 * An environment variable has the form <key>=<value>, followed
				 * by a terminating zero.
				 */
				size_t const var_size = key  .value_size() + 1
				                      + value.value_size() + 1;

				envp[env_i] = (char*)malloc(var_size);

				size_t pos = 0;

				/* append characters to env variable with zero termination */
				auto append = [&] (char const *s, size_t len) {

					if (pos + len >= var_size) {
						/* this should never happen */
						warning("truncated environment variable: ", node);
						return;
					}

					/* Genode's strncpy always zero-terminates */
					Genode::strncpy(envp[env_i] + pos, s, len + 1);
					pos += len;
				};

				key.with_raw_value([&] (char const *start, size_t length) {
					append(start, length); });

				append("=", 1);

				value.with_raw_value([&] (char const *start, size_t length) {
					append(start, length); });

				++env_i;

			} catch (Xml_node::Nonexistent_sub_node) { }
		});

		envp[env_i] = NULL;

		/* register command-line arguments at Genode's startup code */
		genode_argc = argc;
		genode_argv = argv;
		genode_envp = environ = envp;
	});

	exit(main(genode_argc, genode_argv, genode_envp));
}

#include <base/attached_io_mem_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/heap.h>
#include <dmidecode.h>
#include <util/reconstructible.h>
#include <util/fifo.h>
#include <os/reporter.h>

using namespace Genode;

class Dmi_table
{
	public:

		class Header
		{
			private:

				dmi_header     const _header;
				Fifo_element<Header> _fifo_elem { *this };

			public:

				Header(dmi_header const &header)
				:
					_header { header }
				{ }

				dmi_header     const &header() const { return _header; }
				Fifo_element<Header> &fifo_elem()    { return _fifo_elem; };
		};

		class Smbios_version
		{
			private:

				uint8_t const _major;
				uint8_t const _middle;
				uint8_t const _minor;

			public:

				Smbios_version(uint8_t major,
				               uint8_t middle,
				               uint8_t minor)
				:
					_major  { major },
					_middle { middle },
					_minor  { minor }
				{ }

				uint8_t major()  const { return _major; }
				uint8_t middle() const { return _middle; }
				uint8_t minor()  const { return _minor; }
		};

	private:

		/* 16 two-digit hex values, 4 hyphen, terminating null */
		using Uuid_string = String<2 * 16 + 4 + 1>;

		/* address value in hex. 2 chars prefix, terminating null */
		using Addr_string = String<sizeof(addr_t) * 2 + 2 + 1>;

		/* two 0..255 numbers, 1 dot, terminating null*/
		using Version_2_string = String<3 * 2 + 1 + 1>;

		/* three 0..255 numbers, 2 dots, terminating null*/
		using Version_3_string = String<3 * 3 + 2 + 1>;

		/* 64bit value, 2 char unit, terminating null */
		using Size_string = String<20 + 2 + 1>;

		/* 2 digit hex value with padding but w/o prefix */
		struct Uuid_hex : Hex
		{
			Uuid_hex(char digit) : Hex(digit, Hex::OMIT_PREFIX, Hex::PAD) { }
		};

		Allocator                   &_alloc;
		addr_t                 const _base;
		Smbios_version         const _smbios_version;
		Fifo<Fifo_element<Header> >  _headers { };

	public:

		Dmi_table(Allocator      &alloc,
		          addr_t          base,
		          Smbios_version  smbios_version)
		:
			_alloc          { alloc },
			_base           { base },
			_smbios_version { smbios_version }
		{ }

		~Dmi_table()
		{
			_headers.dequeue_all([&] (Fifo_element<Header> &fifo_elem) {
				destroy(&_alloc, &fifo_elem.object());
			});
		}

		void add_header(dmi_header const &raw)
		{
			Header &header { *new (_alloc) Header(raw) };
			_headers.enqueue(header.fifo_elem());
		}

		const char *dmi_string(dmi_header const &header,
		                       uint8_t           value_id)
		{
			if (value_id == 0)
				return "[not specified]";

			char *result = (char *)header.data + header.length;
			while (value_id > 1 && *result) {
				result += strlen(result);
				result++;
				value_id--;
			}

			if (!*result)
				return "[bad index]";

			return result;
		}

		void report_dmi_bios_rom_size(Reporter::Xml_generator &xml,
		                              u8                       code_1,
		                              u16                      code_2)
		{
			xml.node("rom-size", [&] () {
				if (code_1 != 0xff) {
					xml.attribute("value", Size_string(((size_t)code_1 + 1) << 6, " KB"));
					return;
				}
				switch (code_2 >> 14) {
				case  0: xml.attribute("value", Size_string(code_2 & 0x3fff, " MB")); return;
				case  1: xml.attribute("value", Size_string(code_2 & 0x3fff, " GB")); return;
				default: xml.attribute("value", "[bad unit]");                       return;
				}
			});
		}

		void report_dmi_bios_character_0(Reporter::Xml_generator &xml,
		                                 u64                      code)
		{
			static const char *characteristics[] = {
				"BIOS characteristics not supported", /* 3 */
				"ISA is supported",
				"MCA is supported",
				"EISA is supported",
				"PCI is supported",
				"PC Card (PCMCIA) is supported",
				"PNP is supported",
				"APM is supported",
				"BIOS is upgradeable",
				"BIOS shadowing is allowed",
				"VLB is supported",
				"ESCD support is available",
				"Boot from CD is supported",
				"Selectable boot is supported",
				"BIOS ROM is socketed",
				"Boot from PC Card (PCMCIA) is supported",
				"EDD is supported",
				"Japanese floppy for NEC 9800 1.2 MB is supported (int 13h)",
				"Japanese floppy for Toshiba 1.2 MB is supported (int 13h)",
				"5.25&quot;/360 kB floppy services are supported (int 13h)",
				"5.25&quot;/1.2 MB floppy services are supported (int 13h)",
				"3.5&quot;/720 kB floppy services are supported (int 13h)",
				"3.5&quot;/2.88 MB floppy services are supported (int 13h)",
				"Print screen service is supported (int 5h)",
				"8042 keyboard services are supported (int 9h)",
				"Serial services are supported (int 14h)",
				"Printer services are supported (int 17h)",
				"CGA/mono video services are supported (int 10h)",
				"NEC PC-98" /* 31 */
			};
			if (code.l & (1 << 3)) {
				xml.node("characteristic", [&] () {
					xml.attribute("value", characteristics[0]);
				});
				return;
			}
			for (int i = 4; i <= 31; i++) {
				if (code.l & (1 << i)) {
					xml.node("characteristic", [&] () {
						xml.attribute("value", characteristics[i - 3]);
					});
				}
			}
		}

		void report_dmi_bios_character_1(Reporter::Xml_generator &xml,
		                                 u8                       code)
		{
			static const char *characteristics[] = {
				"ACPI is supported", /* 0 */
				"USB legacy is supported",
				"AGP is supported",
				"I2O boot is supported",
				"LS-120 boot is supported",
				"ATAPI Zip drive boot is supported",
				"IEEE 1394 boot is supported",
				"Smart battery is supported" /* 7 */
			};
			for (int i = 0; i <= 7; i++) {
				if (code & (1 << i)) {
					xml.node("characteristic", [&] () {
						xml.attribute("value", characteristics[i]);
					});
				}
			}
		}

		void report_dmi_bios_character_2(Reporter::Xml_generator &xml,
		                                 u8                       code)
		{
			static const char *characteristics[] = {
				"BIOS boot specification is supported", /* 0 */
				"Function key-initiated network boot is supported",
				"Targeted content distribution is supported",
				"UEFI is supported",
				"System is a virtual machine" /* 4 */
			};
			for (int i = 0; i <= 4; i++) {
				if (code & (1 << i)) {
					xml.node("characteristic", [&] () {
						xml.attribute("value", characteristics[i]);
					});
				}
			}
		}

		void report_dmi_string(Reporter::Xml_generator &xml,
		                       dmi_header        const &header,
		                       char              const *type,
		                       unsigned                 value_id)
		{
			xml.node(type, [&] () {
				xml.attribute("value", dmi_string(header, header.data[value_id])); });
		}

		char const *dmi_sys_wake_up_type(uint8_t code)
		{
			switch (code) {
			case 0:  return "Reserved";
			case 1:  return "Other";
			case 2:  return "Unknown";
			case 3:  return "APM Timer";
			case 4:  return "Modem Ring";
			case 5:  return "LAN Remote";
			case 6:  return "Power Switch";
			case 7:  return "PCI PME#";
			case 8:  return "AC Power Restored";
			default: return "[out of spec]";
			}
		}

		void report_dmi_system_uuid(Reporter::Xml_generator &xml,
		                            uint8_t const           *data)
		{
			bool only_zeros { true };
			bool only_ones  { true };
			for (unsigned off = 0; off < 16 && (only_zeros || only_ones); off++)
			{
				if (data[off] != 0x00) {
					only_zeros = false;
				}
				if (data[off] != 0xff) {
					only_ones = false;
				}
			}

			xml.node("uuid", [&] () {
				if (only_ones) {
					xml.attribute("value", "[not present]");
					return;
				}
				if (only_zeros) {
					xml.attribute("value", "[not settable]");
					return;
				}
				if (_smbios_version.major()  >  2 ||
					(_smbios_version.major()  == 2 &&
					 _smbios_version.middle() >= 6))
				{
					xml.attribute("value", Uuid_string(
						Uuid_hex(data[3]), Uuid_hex(data[2]),
						Uuid_hex(data[1]), Uuid_hex(data[0]),
						"-",
						Uuid_hex(data[5]), Uuid_hex(data[4]),
						"-",
						Uuid_hex(data[7]), Uuid_hex(data[6]),
						"-",
						Uuid_hex(data[8]), Uuid_hex(data[9]),
						"-",
						Uuid_hex(data[10]), Uuid_hex(data[11]),
						Uuid_hex(data[12]), Uuid_hex(data[13]),
						Uuid_hex(data[14]), Uuid_hex(data[15])));

				} else {
					xml.attribute("value", Uuid_string(
						Uuid_hex(data[0]), Uuid_hex(data[1]),
						Uuid_hex(data[2]), Uuid_hex(data[3]),
						"-",
						Uuid_hex(data[4]), Uuid_hex(data[5]),
						"-",
						Uuid_hex(data[6]), Uuid_hex(data[7]),
						"-",
						Uuid_hex(data[8]), Uuid_hex(data[9]),
						"-",
						Uuid_hex(data[10]), Uuid_hex(data[11]),
						Uuid_hex(data[12]), Uuid_hex(data[13]),
						Uuid_hex(data[14]), Uuid_hex(data[15])));
				}
			});
		}

		void report_dmi_str(Reporter::Xml_generator &xml,
		                    char              const *type,
		                    char              const *value)
		{
			xml.node(type, [&] () { xml.attribute("value", value); });
		}

		void report_sys_info(Reporter::Xml_generator &xml,
		                     dmi_header        const &header)
		{
			uint8_t const *data { header.data };
			xml.attribute("name", "System Information");
			if (header.length < 8) {
				return; }

			report_dmi_string(xml, header, "manufacturer",  4);
			report_dmi_string(xml, header, "product-name",  5);
			report_dmi_string(xml, header, "version",       6);
			report_dmi_string(xml, header, "serial-number", 7);
			if (header.length < 25) {
				return; }

			report_dmi_system_uuid(xml, data + 8);
			report_dmi_str(xml, "wake-up-type", dmi_sys_wake_up_type(data[24]));
			if (header.length < 27) {
				return; }

			report_dmi_string(xml, header, "sku-number", 25);
			report_dmi_string(xml, header, "family",     26);
		}

		void report_bios_info(Reporter::Xml_generator &xml,
		                      dmi_header        const &header)
		{
			uint8_t const *data { header.data };
			xml.attribute("name", "BIOS Information");
			if (header.length < 18) {
				return; }

			report_dmi_string(xml, header, "vendor",       4);
			report_dmi_string(xml, header, "version",      5);
			report_dmi_string(xml, header, "release-date", 8);
			{
				addr_t const code { WORD(data + 6) };
				if (code) {
					xml.node("address", [&] () {
						xml.attribute("value", Addr_string(Hex(code << 4))); });

					xml.node("runtime-size", [&] () {
						xml.attribute("value", (0x10000 - code) << 4); });
				}
			}
			{
				u16 code_2;
				if (header.length < 26) {
					code_2 = 16;
				} else {
					code_2 = WORD(data + 24);
				}
				report_dmi_bios_rom_size(xml, data[9], code_2);
			}
			report_dmi_bios_character_0(xml, QWORD(data + 10));
			if (header.length < 0x13) {
				return; }

			report_dmi_bios_character_1(xml, data[0x12]);
			if (header.length < 0x14) {
				return; }

			report_dmi_bios_character_2(xml, data[0x13]);
			if (header.length < 0x18) {
				return; }

			if (data[20] != 0xff && data[21] != 0xff) {
				xml.node("bios-revision", [&] () {
					xml.attribute("value", Version_2_string(data[20], ".", data[21]));
				});
			}
			if (data[22] != 0xff && data[23] != 0xff) {
				xml.node("firmware-revision", [&] () {
					xml.attribute("value", Version_2_string(data[22], ".", data[23]));
				});
			}
		}

		void report_dmi_base_board_features(Reporter::Xml_generator &xml,
		                                    uint8_t                  code)
		{
			static const char *features[] = {
				"Board is a hosting board",
				"Board requires at least one daughter board",
				"Board is removable",
				"Board is replaceable",
				"Board is hot swappable"
			};
			if ((code & 0x1F) == 0) {
				report_dmi_str(xml, "feature", "[none]");
			} else {
				for (unsigned id = 0; id < sizeof(features) / sizeof(features[0]); id++) {
					if (code & (1 << id)) {
						report_dmi_str(xml, "feature", features[id]);
					}
				}
			}
		}

		const char *dmi_base_board_type(uint8_t code)
		{
			switch (code) {
			case  1: return "Unknown";
			case  2: return "Other";
			case  3: return "Server Blade";
			case  4: return "Connectivity Switch";
			case  5: return "System Management Module";
			case  6: return "Processor Module";
			case  7: return "I/O Module";
			case  8: return "Memory Module";
			case  9: return "Daughter Board";
			case 10: return "Motherboard";
			case 11: return "Processor+Memory Module";
			case 12: return "Processor+I/O Module";
			case 13: return "Interconnect Board";
			default: return "[out of spec]";
			}
		}

		void report_dmi_base_board_handles(Reporter::Xml_generator &xml,
		                                   uint8_t                  count,
		                                   uint8_t           const *data)
		{
			for (unsigned id = 0; id < count; id++) {
				xml.node("contained-object-handle", [&] () {
					xml.attribute("value", Addr_string(WORD(data + sizeof(u16) * id)));
				});
			}
		}

		void report_base_board(Reporter::Xml_generator &xml,
		                       dmi_header        const &header)
		{
			xml.attribute("name", "Base Board Information");
			if (header.length < 8) {
				return; }

			report_dmi_string(xml, header, "manufacturer",  4);
			report_dmi_string(xml, header, "product-name",  5);
			report_dmi_string(xml, header, "version",       6);
			report_dmi_string(xml, header, "serial-number", 7);
			if (header.length < 9) {
				return; }

			report_dmi_string(xml, header, "asset-tag", 8);
			if (header.length < 10) {
				return; }

			uint8_t const *data { header.data };
			report_dmi_base_board_features(xml, data[9]);
			if (header.length < 14) {
				return; }

			report_dmi_string(xml, header, "location-in-chassis", 10);

			xml.node("chassis-handle", [&] () {
				xml.attribute("value", WORD(data + 11)); });

			report_dmi_str(xml, "type", dmi_base_board_type(data[13]));
			if (header.length < 15) {
				return; }

			if (header.length < 15 + data[14] * sizeof(u16)) {
				return; }

			report_dmi_base_board_handles(xml, data[14], data + 15);
		}

		void report_header(Reporter::Xml_generator &xml,
		                   dmi_header        const &header)
		{
			xml.node("structure", [&] () {
				xml.attribute("handle", header.handle);
				xml.attribute("type",   header.type);
				xml.attribute("length", header.length);
				switch (header.type) {
				case 0: report_bios_info(xml, header);  break;
				case 1: report_sys_info(xml, header);   break;
				case 2: report_base_board(xml, header); break;
				default: break;
				}
			});
		}

		void report(Reporter::Xml_generator &xml)
		{
			xml.node("table", [&] () {
				xml.attribute("base", Addr_string(Hex(_base)));
				xml.attribute("smbios-version",
					Version_3_string(_smbios_version.major(), ".",
					                 _smbios_version.middle(), ".",
					                 _smbios_version.minor()));

				_headers.for_each([&] (Fifo_element<Header> &fifo_elem) {
					report_header(xml, fifo_elem.object().header());
				});
			});
		}
};

struct Memory_chunk
{
	Allocator                  &alloc;
	size_t               const  size;
	void                       *base { alloc.alloc(size) };
	Fifo_element<Memory_chunk>  fifo_elem { *this };

	Memory_chunk(Allocator &alloc,
	             size_t     size)
	:
		alloc { alloc },
		size  { size }
	{ }

	~Memory_chunk() { alloc.free(base, size); }
};

struct Genode_environment
{
	enum { MEM_CHUNK_BASE = 0xe0000 };
	enum { MEM_CHUNK_SIZE = 0x20000 };

	Env                               &env;
	Genode::Heap                       heap       { env.ram(), env.rm() };
	Constructible<Dmi_table>           dmi_table  { };
	Expanding_reporter                 reporter   { env, "result", "result" };
	Fifo<Fifo_element<Memory_chunk> >  mem_chunks { };

	Genode_environment(Env &env) : env { env } { }

	~Genode_environment()
	{
		if (dmi_table.constructed()) {
			reporter.generate([&] (Reporter::Xml_generator &xml) {
				dmi_table->report(xml);
			});
		}
		mem_chunks.dequeue_all([&] (Fifo_element<Memory_chunk> &fifo_elem) {
			destroy(&heap, &fifo_elem.object());
		});
	}
};

static Constructible<Genode_environment> ge;

void Libc::Component::construct(Libc::Env &env)
{
	env.exec_static_constructors();
	ge.construct(env);

	Libc::with_libc([&] () { construct_component(env); });

	ge.destruct();
}


extern "C" void *genode_mem_chunk(size_t base, size_t len, const char *devmem)
{

	addr_t const base_aligned { base & ~(((addr_t)1 << 12) - 1) };
	addr_t const base_off     { base - base_aligned };

	Memory_chunk &mem_chunk { *new (ge->heap) Memory_chunk(ge->heap, len) };
	ge->mem_chunks.enqueue(mem_chunk.fifo_elem);

	Attached_io_mem_dataspace mmio { ge->env, base_aligned, len };
	memcpy(mem_chunk.base, (void *)(((addr_t)mmio.local_addr<char>()) + base_off), len);
	return mem_chunk.base;
}

extern "C" void genode_dmi_header(dmi_header const *header)
{
	ge->dmi_table->add_header(*header);
}

extern "C" void genode_dmi_table(addr_t   base,
                                 uint32_t smbios_version)
{
	struct Dmi_table_already_announced : Exception { };
	if (ge->dmi_table.constructed()) {
		throw Dmi_table_already_announced();
	}
	Dmi_table::Smbios_version smbver {
		(uint8_t)((smbios_version >> 16) & 0xff),
		(uint8_t)((smbios_version >> 8)  & 0xff),
		(uint8_t)((smbios_version)       & 0xff),
	};
	ge->dmi_table.construct(ge->heap, base, smbver);
}
