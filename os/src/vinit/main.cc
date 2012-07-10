/*
 * \brief  Init process that enables device emulation
 * \author Martin Stein
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <root/client.h>
#include <rm_session/client.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <emulation_session/client.h>

/* local includes */
#include <emulated_child.h>
#include <io_mem_session/root.h>
#include <irq_session/root.h>
#include <cpu_session/connection.h>
#include <rm_session/connection.h>

namespace Init
{
	using namespace Genode;

	bool config_verbose = false;


	/* vinit begin */
	class Emulation_context;

	/**
	 * Holds informations about an emulated resource-region
	 */
	class Emulated_region : public Avl_node<Emulated_region>
	{
		addr_t const _base;
		addr_t const _end;
		addr_t const _local;
		Emulation_context * const _emu_context;

		public:

			/**
			 * Constructor
			 *
			 * For parameter description see same-named members.
			 */
			Emulated_region(addr_t const base, size_t const size,
			                addr_t const local,
			                Emulation_context * const emu_context)
			:
				_base(base), _end(base + size), _local(local),
				_emu_context(emu_context)
			{ }

			/**
			 * Lookup region within this AVL subtree that covers 'addr'
			 */
			Emulated_region * find_by_addr(addr_t const addr)
			{
				if ((addr >= _base) && (addr < _end)) return this;
				Emulated_region * region = child(addr > _base);
				return region ? region->find_by_addr(addr) : 0;
			}

			bool intersect_exists(addr_t const base,
			                      addr_t const end)
			{
				if ((base < _end) && (end > _base)) return 1;
				Emulated_region * region = child(base > _base);
				return region ? region->intersect_exists(base, end) : 0;
			}

			/***************
			 ** Accessors **
			 ***************/

			addr_t base() const { return _base; }
			addr_t end() const { return _end; }
			addr_t local() const { return _local; }
			Emulation_context * emu_context() const { return _emu_context; }

			/**************
			 ** Avl_node **
			 **************/

			bool higher(Emulated_region * e) { return e->_base > _base; }
	};

	/**
	 * Synchronous map of all emulated regions with the same resource type
	 */
	class Emulated_regions
	{
		Avl_tree<Emulated_region> _tree; /* maps adresses to regions */
		Lock _tree_lock; /* sync access to '_tree' */

		public:

			Emulated_regions() : _tree_lock(Lock::UNLOCKED) { }

			/**
			 * Find emulated region that covers 'addr'
			 */
			Emulated_region * find_by_addr(addr_t const addr)
			{
				Lock::Guard lock_guard(_tree_lock);
				Emulated_region * region = _tree.first();
				return region ? region->find_by_addr(addr) : 0;
			}

			bool intersect_exists(addr_t const base,
			                      size_t const end)
			{
				Lock::Guard lock_guard(_tree_lock);
				Emulated_region * region = _tree.first();
				return region ? region->intersect_exists(base, end) : 0;
			}

			void insert(Emulated_region * const region)
			{
				if (intersect_exists(region->base(), region->end())) {
					PERR("%s:%d: Emulated regions overlap",
					     __FILE__, __LINE__);
					sleep_forever();
				}

				Lock::Guard lock_guard(_tree_lock);
				_tree.insert(region);
			}
	};

	/**
	 * Map of all emulated 'IO_MEM' regions
	 */
	static Emulated_regions * emulated_io_mem()
	{
		static Emulated_regions _o;
		return &_o;
	}

	/**
	 * Map of all emulated 'IRQ' regions
	 */
	static Emulated_regions * emulated_irq()
	{
		static Emulated_regions _o;
		return &_o;
	}
	/* vinit end */


	/* vinit begin */
	class Emulator_child : public Emulated_child,
	                       public Emulator_childs::Entry
	{
		enum {
			SESSION_ARGS_SIZE = 32,
			SESSION_RAM = 8*1024,
		};

		Lock _service_announced;
		Root_capability _root;
		Root_client * _root_client;
		Emulation::Session * _session;

		public:

			Emulator_child(
			      Xml_node                  emulator_node,
			      Xml_node                  default_route_node,
			      Name_registry * const     name_registry,
			      long const                prio_levels_log2,
			      Service_registry * const  parent_services,
			      Service_registry * const  child_services,
			      Cap_session * const       cap_session,
			      Cpu_root * const          cpu_root,
			      Rm_root * const           rm_root,
			      Emulation_context * const emu_context,
			      Service_registry * const  spy_services,
			      Service_registry * const  emulated_services)
			:
				Emulated_child(emulator_node, default_route_node,
				               name_registry, prio_levels_log2,
				               parent_services, child_services,
				               cap_session, cpu_root, rm_root,
				               spy_services, emulated_services),
				Emulator_childs::Entry(emu_context),
				_service_announced(Lock::LOCKED)
			{
				/* start child and await anouncement of emulation service */
				start();
				_service_announced.lock();

				/* create a session to the childs emulation service */
				_root_client = new (env()->heap()) Root_client(_root);
				char args[SESSION_ARGS_SIZE];
				snprintf(args, sizeof(args), "ram_quota=%i", SESSION_RAM);
				Emulation::Session_capability cap;
				cap = static_cap_cast<Emulation::Session>(_root_client->session(args));
				_session = new (env()->heap()) Emulation::Session_client(cap);
			}

			Emulation::Session * session() const { return _session; }

			/****************************
			 ** Child-policy interface **
			 ****************************/

			virtual bool announce_service(const char * service_name,
			                              Root_capability root,
			                              Allocator * alloc,
			                              Server * server)
			{
				if (strcmp(service_name, Emulation::Session::service_name())) {
					PERR("%s:%d: Unexpected service announcement", __FILE__, __LINE__);
					return 0;
				}
				_root = root;
				_service_announced.unlock();
				return 1;
			}
	};

	const char * emulation_key()
	{ return Emulation::Session::service_name(); }

	enum { MAX_EMULATOR_CHILDS = 1024 };

	/**
	 * Set of resources that are assigned to one type of emulator
	 *
	 * Multiple emulation contexts can be assigned to the same
	 * type of emulator. In the normal case the contained resources
	 * should fit the resources that are managed by the emulator.
	 * However, the result of access to any resource within an emulation
	 * context depends on the implementation of the emulator it is
	 * assigned to.
	 */
	class Emulation_context
	{
		Xml_node _emulator_node;
		Allocator * const _md_alloc;

		public:

			Emulation_context(Xml_node xml_node, Xml_node emulator_node,
			                  Allocator * const md_alloc)
			: _emulator_node(emulator_node), _md_alloc(md_alloc)
			{
				/* look up resources that the emulation context contains */
				Xml_node resource("<empty/>");
				try { resource = xml_node.sub_node("resource"); }
				catch (...) { assert(0); }
				while (1) {
					try
					{
						/* get emulated-region map by resource type */
						Emulated_regions * regions = 0;
						if (resource.attribute("name").has_value("IO_MEM"))
							regions = emulated_io_mem();
						else if (resource.attribute("name").has_value("IRQ"))
							regions = emulated_irq();
						if (!regions) assert(0);

						/* remember the resource region */
						addr_t base;
						size_t size;
						addr_t local;
						resource.attribute("base").value(&base);
						resource.attribute("size").value(&size);
						resource.attribute("local").value(&local);
						Emulated_region * region;
						region = new (_md_alloc)
							Emulated_region(base, size, local, this);
						regions->insert(region);
					} catch (...) { assert(0); }

					/* get next resource or leave */
					try { resource = resource.next("resource");
					} catch (...) { break; }
				}
			}

			Xml_node emulator_node() const { return _emulator_node; }
	};

	typedef Genode::List<Genode::List_element<Emulated_child> > Child_list;

	class Child_registry : public Name_registry, Child_list
	{
		public:

			/**
			 * Register child
			 */
			void insert(Emulated_child * child)
			{
				Child_list::insert(&child->_list_element);
			}

			/**
			 * Start execution of all children
			 */
			void start() {
				Genode::List_element<Emulated_child> *curr = first();
				for (; curr; curr = curr->next())
					curr->object()->start();
			}


			/*****************************
			 ** Name-registry interface **
			 *****************************/

			bool is_unique(const char *name) const
			{
				Genode::List_element<Emulated_child> *curr = first();
				for (; curr; curr = curr->next())
					if (curr->object()->has_name(name))
						return false;

				return true;
			}

			Genode::Server *lookup_server(const char *name) const
			{
				Genode::List_element<Emulated_child> *curr = first();
				for (; curr; curr = curr->next())
					if (curr->object()->has_name(name))
						return curr->object()->server();

				return 0;
			}
	};


	void Emulated_child::_interpose_emulation(const char * service,
	                                          char * args, size_t args_len)
	{
		/* avoid false emulation requests */
		assert(!Arg_string::find_arg(args, emulation_key()).valid());

		/* check if request targets resource that might be emulated */
		addr_t base;
		size_t size;
		enum { MAX_LOCAL_KEY_SIZE = 32 };
		char local_key[MAX_LOCAL_KEY_SIZE];
		Emulated_regions * regions = 0;
		if (!strcmp(service, "IO_MEM"))
		{
			/* get region map and targeted address range */
			snprintf(local_key, sizeof(local_key), "base");
			regions = emulated_io_mem();
			Arg base_arg = Arg_string::find_arg(args, local_key);
			Arg size_arg = Arg_string::find_arg(args, "size");
			assert(base_arg.valid() && size_arg.valid());
			base = base_arg.ulong_value(0);
			size = size_arg.ulong_value(0);
		} else if (!strcmp(service, "IRQ")) {

			/* get region map and targeted address range */
			snprintf(local_key, sizeof(local_key), "irq_number");
			regions = emulated_irq();
			Arg irq_number = Arg_string::find_arg(args, local_key);
			assert(irq_number.valid());
			base = irq_number.ulong_value(0);
			size = 1;
		}
		if (!regions) { return; }

		/* check if request fits an emulated region exactly */
		Emulated_region * const region = regions->find_by_addr(base);
		if (!region)
		{
			/* we attempt to hand off the request to a native provider,
			 * thus we must ensure that the requested address range
			 * doesn't even overlap a region that is emulated by us */
			assert(!regions->find_by_addr(base + size - 1));
			return;
		}
		assert(region->base() == base && region->end() == base + size);

		/* look for an existing emulator for this context and child */
		Emulator_child * emu_child =
			_emulator_childs.find_by_context(region->emu_context());
		if (!emu_child)
		{
			/* create and remember an emulator child for the context */
			try {
				emu_child = new (env()->heap())
					Emulator_child(region->emu_context()->emulator_node(),
					               _default_route_node, _name_registry,
					               _resources.prio_levels_log2,
					               _parent_services, _child_services,
					               _cap_session, _cpu_root, _rm_root,
					               region->emu_context(),
					               _spy_services, _emulated_services);
				_emulator_childs.insert(emu_child);
			} catch (...) { assert(0); }
		}

		/* redirect the request routing to the emulated service */
		enum { HEX_TO_ASCII_SIZE_FACTOR = 2 };
		Emulation::Session * const emu_session = emu_child->session();
		char value[HEX_TO_ASCII_SIZE_FACTOR * sizeof(emu_session) +
		           sizeof("0x")];
		snprintf(value, sizeof(value), "0x%p", emu_session);
		Arg_string::set_arg(args, args_len, emulation_key(), value);
		Arg_string::set_arg(args, args_len, local_key, region->local());
	}
	/* vinit end */


	/**
	 * Read priority-levels declaration from config file
	 */
	inline long read_prio_levels_log2()
	{
		using namespace Genode;

		long prio_levels = 0;
		try {
			config()->xml_node().attribute("prio_levels").value(&prio_levels); }
		catch (...) { }

		if (prio_levels && prio_levels != (1 << log2(prio_levels))) {
			printf("Warning: Priolevels is not power of two, priorities are disabled\n");
			prio_levels = 0;
		}
		return prio_levels ? log2(prio_levels) : 0;
	}


	/**
	 * Read parent-provided services from config file
	 */
	inline void determine_parent_services(Service_registry *services)
	{
		using namespace Genode;

		if (Init::config_verbose)
			printf("parent provides\n");

		Xml_node node = config()->xml_node().sub_node("parent-provides").sub_node("service");
		for (; ; node = node.next("service")) {

			char service_name[Service::MAX_NAME_LEN];
			node.attribute("name").value(service_name, sizeof(service_name));

			Parent_service *s = new (env()->heap()) Parent_service(service_name);
			services->insert(s);
			if (Init::config_verbose)
				printf("  service \"%s\"\n", service_name);

			if (node.is_last("service")) break;
		}
	}

	void determine_emulated_services(Xml_node default_route, Allocator * const md_alloc)
	{
		enum { EMULATOR_NAME_SIZE = 64 };

		/* look for available emulators and assigned emulation contexts */
		try {
			Xml_node emu_node = config()->xml_node().sub_node("emulator");
			for (; ; emu_node = emu_node.next("emulator"))
			{
				/* get the name of the emulator */
				char emu_name[EMULATOR_NAME_SIZE];
				emu_name[0] = 0;
				try { emu_node.attribute("name")
							  .value(emu_name, sizeof(emu_name)); }
				catch(...) {
					PWRN("%s:%d: Invalid syntax", __FILE__, __LINE__);
					continue;
				}

				/* look up emulation contexts that are assigned to the emulator */
				Xml_node emu_context = config()->xml_node().sub_node("emulated");
				try {
					for (; ; emu_context = emu_context.next("emulated"))
					{
						/* check if context is assigned to the emulator */
						try {
							if (!emu_context.attribute("by").has_value(emu_name))
								continue;
						}
						catch(...) {
							PWRN("%s:%d: Invalid syntax", __FILE__, __LINE__);
							continue;
						}

						/*
						 * Create context and emulator if they fit together.
						 * Both are remembered through refernces that are contained
						 * by the 'Emulated_region' objects, wich are created by
						 * the constructor of 'Emulation_context'.
						 */
						try {
							new (md_alloc)
								Emulation_context(emu_context, emu_node,
								                  md_alloc);
						} catch (...) { assert(0); }
					}
				} catch (...) { }
			}
		} catch (...) { }
	}
}

int main(int, char **)
{
	using namespace Init;

	try {
		config_verbose =
			config()->xml_node().attribute("verbose").has_value("yes"); }
	catch (...) { }

	/* look for dynamic linker */
	try {
		static Rom_connection rom("ld.lib.so");
		Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	static Cap_connection   cap;

	/* vinit begin */
	enum { ENTRYPOINT_STACK_SIZE = 8*1024 };

	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());
	static Rpc_entrypoint   ep(&cap, ENTRYPOINT_STACK_SIZE, "entrypoint");
	static Rpc_entrypoint   io_mem_ep(&cap, ENTRYPOINT_STACK_SIZE, "io_mem_entrypoint");
	static Service_registry spy_services;
	static Service_registry emulated_services;
	/* vinit end */

	static Service_registry parent_services;
	static Service_registry child_services;
	static Child_registry   children;

	/* vinit begin */
	static Cpu_root cpu_root(&ep, &sliced_heap);
	static Local_service cpu(Cpu_session::service_name(), &cpu_root);
	spy_services.insert(&cpu);

	static Rm_root rm_root(&ep, &sliced_heap);
	static Local_service rm(Rm_session::service_name(), &rm_root);
	spy_services.insert(&rm);

	static Io_mem_root io_mem_root(&io_mem_ep, &sliced_heap, &rm_root);
	static Local_service io_mem(Io_mem_session::service_name(), &io_mem_root);
	emulated_services.insert(&io_mem);

	static Irq_root irq_root(&cap, &sliced_heap);
	static Local_service irq(Irq_session::service_name(), &irq_root);
	emulated_services.insert(&irq);
	/* vinit end */

	try { determine_parent_services(&parent_services); }
	catch (...) { }

	/* determine default route for resolving service requests */
	Xml_node default_route_node("<empty/>");
	try {
		default_route_node =
			config()->xml_node().sub_node("default-route"); }
	catch (...) { }

	/* vinit begin */
	determine_emulated_services(default_route_node, &sliced_heap);
	/* vinit end */

	/* create children */
	try {
		Xml_node start_node = config()->xml_node().sub_node("start");
		for (;; start_node = start_node.next("start")) {

			Emulated_child * const child =
				new (env()->heap())
					Emulated_child(start_node, default_route_node, &children,
					               read_prio_levels_log2(), &parent_services,
					               &child_services, &cap,

					               /* vinit begin */
					               &cpu_root, &rm_root, &spy_services,
					               &emulated_services);
					               /* vinit end */
			children.insert(child);
			if (start_node.is_last("start")) break;
		}
	}
	catch (Xml_node::Nonexistent_sub_node) {
		PERR("No children to start");
	}

	/* start children */
	children.start();

	sleep_forever();
	return 0;
}

