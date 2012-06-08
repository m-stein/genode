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
#include <base/signal.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <base/child.h>
#include <root/component.h>
#include <root/client.h>
#include <rm_session/client.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <cap_session/connection.h>
#include <io_mem_session/io_mem_session.h>
#include <emulator_session/capability.h>
#include <emulator_session/client.h>
#include <os/config.h>

/* init includes */
#include <init/child_config.h>
#include <init/child_policy.h>

/**
 * Assert a condition
 *
 * \param  expression  Expression that must be true
 */
#define assert(expression) \
	do { \
		if (!(expression)) { \
			PERR("Assertion failed: "#expression""); \
			PERR("  File: %s:%d", __FILE__, __LINE__); \
			PERR("  Function: %s", __PRETTY_FUNCTION__); \
			while (1) ; \
		} \
	} while (0) ;


namespace Init
{
	/* import types from Genode */
	typedef Genode::addr_t addr_t;
	typedef Genode::size_t size_t;
	typedef Genode::Xml_node Xml_node;
	typedef Genode::Number_of_bytes Number_of_bytes;
	typedef Genode::Cpu_session Cpu_session;
	typedef Genode::Cpu_connection Cpu_connection;
	typedef Genode::Ram_connection Ram_connection;
	typedef Genode::Arg_string Arg_string;
	typedef Genode::Service Service;
	typedef Genode::Service_registry Service_registry;
	typedef Genode::Session_capability Session_capability;
	typedef Genode::Root_capability Root_capability;
	typedef Genode::Allocator Allocator;
	typedef Genode::Rpc_entrypoint Rpc_entrypoint;
	typedef Genode::Sliced_heap Sliced_heap;
	typedef Genode::Cap_connection Cap_connection;
	typedef Genode::Server Server;
	typedef Genode::Signal_receiver Signal_receiver;
	typedef Genode::Signal_context Signal_context;
	typedef Genode::Signal_context_capability Signal_context_capability;

	bool config_verbose = false;

	enum {
		MAX_RM_CLIENTS = 1024,
		MAX_EMULATORS = 1024,
	};


	inline long read_priority(Xml_node start_node)
	{
		long priority = Cpu_session::DEFAULT_PRIORITY;
		try { start_node.attribute("priority").value(&priority); }
		catch (...) { }

		/*
		 * All priority declarations in the config file are
		 * negative because child priorities can never be higher
		 * than parent priorities. To simplify priority
		 * calculations, we use inverted values. Lower values
		 * correspond to higher priorities.
		 */
		return -priority;
	}


	inline size_t read_ram_quota(Xml_node start_node)
	{
		Number_of_bytes ram_quota = 0;
		try {
			Xml_node rsc = start_node.sub_node("resource");
			for (;; rsc = rsc.next("resource")) {

				try {
					if (rsc.attribute("name").has_value("RAM")) {
						rsc.attribute("quantum").value(&ram_quota);
					}
				} catch (...) { }
			}
		} catch (...) { }

		/*
		 * If the configured quota exceeds our own quota, we donate
		 * all remaining quota to the child but we need to count in
		 * our allocation of the child meta data from the heap.
		 * Hence, we preserve some of our own quota.
		 */
		if (ram_quota > Genode::env()->ram_session()->avail() - 128*1024) {
			ram_quota = Genode::env()->ram_session()->avail() - 128*1024;
			if (config_verbose)
				Genode::printf("Warning: Specified quota exceeds available quota.\n"
				               "         Proceeding with a quota of %zd bytes.\n",
				               (size_t)ram_quota);
		}
		return ram_quota;
	}


	/**
	 * Return true if service XML node matches the specified service name
	 */
	inline bool service_node_matches(Xml_node service_node, const char *service_name)
	{
		if (service_node.has_type("any-service"))
			return true;

		return service_node.has_type("service")
		    && service_node.attribute("name").has_value(service_name);
	}


	/**
	 * Check if arguments satisfy the condition specified for the route
	 */
	inline bool service_node_args_condition_satisfied(Xml_node service_node,
	                                                  const char *args)
	{
		try {
			Xml_node if_arg = service_node.sub_node("if-arg");
			enum { KEY_MAX_LEN = 64, VALUE_MAX_LEN = 64 };
			char key[KEY_MAX_LEN];
			char value[VALUE_MAX_LEN];
			if_arg.attribute("key").value(key, sizeof(key));
			if_arg.attribute("value").value(value, sizeof(value));

			char arg_value[VALUE_MAX_LEN];
			Arg_string::find_arg(args, key).string(arg_value, sizeof(arg_value), "");
			return Genode::strcmp(value, arg_value) == 0;
		} catch (...) { }

		/* if no if-arg node exists, the condition is met */
		return true;
	}


	/**
	 * Init-specific representation of a child service
	 *
	 * For init, we introduce this 'Service' variant that distinguishes two
	 * phases, declared and announced. A 'Routed_service' object is created
	 * when a '<provides>' declaration is found in init's configuration.
	 * At that time, however, no children including the server do yet exist.
	 * If, at this stage, a client tries to open a session to this service,
	 * the client get enqueued in a list of applicants and blocked. When
	 * the server officially announces its service and passes over the root
	 * capability, the 'Routed_service' enters the announced stage and any
	 * applicants get unblocked.
	 */
	class Routed_service : public Service
	{
		private:

			Root_capability _root;
			bool                    _announced;
			Server         *_server;

			struct Applicant : public Genode::Cancelable_lock,
			                   public Genode::List<Applicant>::Element
			{
				Applicant() : Cancelable_lock(Genode::Lock::LOCKED) { }
			};

			Genode::Lock            _applicants_lock;
			Genode::List<Applicant> _applicants;

		public:

			/**
			 * Constructor
			 *
			 * \param name    name of service
			 * \param server  server providing the service
			 */
			Routed_service(const char     *name,
			               Server *server)
			: Service(name), _announced(false), _server(server) { }

			Server *server() const { return _server; }

			void announce(Root_capability root)
			{
				Genode::Lock::Guard guard(_applicants_lock);

				_root = root;
				_announced = true;

				/* wake up aspiring clients */
				for (Applicant *a; (a = _applicants.first()); ) {
					_applicants.remove(a);
					a->unlock();
				}
			}

			Session_capability session(const char *args)
			{
				/*
				 * This function is called from the context of the client's
				 * activation thread. If the service is not yet announced,
				 * we let the client block.
				 */
				_applicants_lock.lock();
				if (!_announced) {
					Applicant myself;
					_applicants.insert(&myself);
					_applicants_lock.unlock();
					myself.lock();
				} else
					_applicants_lock.unlock();

				Session_capability cap;
				try { cap = Genode::Root_client(_root).session(args); }
				catch (Genode::Root::Invalid_args)   { throw Invalid_args();   }
				catch (Genode::Root::Unavailable)    { throw Unavailable();    }
				catch (Genode::Root::Quota_exceeded) { throw Quota_exceeded(); }

				if (!cap.valid())
					throw Unavailable();

				return cap;
			}

			void upgrade(Session_capability sc, const char *args)
			{
				Genode::Root_client(_root).upgrade(sc, args);
			}

			void close(Session_capability sc)
			{
				Genode::Root_client(_root).close(sc);
			}
	};

	/**
	 * Interface for name database
	 */
	struct Name_registry
	{
		virtual ~Name_registry() { }

		/**
		 * Check if specified name is unique
		 *
		 * \return false if name already exists
		 */
		virtual bool is_unique(const char *name) const = 0;

		/**
		 * Find server with specified name
		 */
		virtual Server *lookup_server(const char *name) const = 0;
	};

	/**
	 * Map unique sortable IDs to object pointers
	 *
	 * \param  OBJECT_T  object type that should be inherited
	 *                   from 'Object_pool::Entry'
	 */
	template <typename _OBJECT_T>
	class Object_pool
	{
		typedef _OBJECT_T Object;

		public:

			enum { INVALID_ID = 0 };

			/**
			 * Objects that shall be managed by the object pool, should
			 * inherit from this
			 */
			class Entry : public Genode::Avl_node<Entry>
			{
				protected:

					unsigned _id;

				public:

					/**
					 * Constructors
					 */
					Entry(unsigned const id) : _id(id) { }

					/**
					 * Find entry with 'object_id' within this AVL subtree
					 */
					Entry * find(unsigned const object_id)
					{
						if (object_id == id()) return this;

						Entry * const subtree = child(object_id > id());

						return subtree ? subtree->find(object_id) : 0;
					}

					/**
					 * ID of this object
					 */
					unsigned const id() const { return _id; }

					/************************
					 * 'Avl_node' interface *
					 ************************/

					bool higher(Entry *e) { return e->id() > id(); }
			};

		private:

			Genode::Avl_tree<Entry> _tree;

		public:

			/**
			 * Add 'object' to pool
			 */
			void insert(Object * const object) { _tree.insert(object); }

			/**
			 * Remove 'object' from pool
			 */
			void remove(Object * const object) { _tree.remove(object); }

			/**
			 * Lookup object
			 */
			Object * object(unsigned const id)
			{
				Entry * object = _tree.first();
				return (Object *)(object ? object->find(id) : 0);
			}
	};

	/**
	 * Manage allocation of a static set of IDs
	 *
	 * \param  _SIZE  How much IDs shall be assignable simultaneously
	 */
	template <unsigned _SIZE>
	class Id_allocator
	{
		enum { MIN = 1, MAX = _SIZE };

		bool _free[MAX + 1]; /* Assignability bitmap */
		unsigned _first_free_id; /* Hint to optimze access */

		/**
		 * Update first free ID after assignment
		 */
		void _first_free_id_assigned()
		{
			_first_free_id++;
			while (_first_free_id <= MAX) {
				if(_free[_first_free_id]) break;
				_first_free_id++;
			}
		}

		/**
		 * Validate ID
		 */
		bool _valid_id(unsigned const id) const { return id >= MIN && id <= MAX; }

		public:

			/**
			 * Constructor, make all IDs unassigned
			 */
			Id_allocator() : _first_free_id(MIN) {
				for (unsigned i = MIN; i <= MAX; i++) _free[i] = 1; }

			/**
			 * Allocate an unassigned ID
			 *
			 * \return  ID that has been allocated by the call
			 */
			unsigned alloc()
			{
				if (!_valid_id(_first_free_id)) assert(0);
				_free[_first_free_id] = 0;
				unsigned const id = _first_free_id;
				_first_free_id_assigned();
				return id;
			}

			/**
			 * Free a given ID
			 */
			void free(unsigned const id)
			{
				if (!_valid_id(id)) return;
				_free[id] = 1;
				if (id < _first_free_id) _first_free_id = id;
			}
	};

	/**
	 * Holds informations about a client of the eveasdropping RM service
	 */
	class Rm_client : public Object_pool<Rm_client>::Entry
	{
		typedef Id_allocator<MAX_RM_CLIENTS> Id_alloc;

		static Id_alloc * _id_alloc()
		{
			static Id_alloc _id_alloc;
			return &_id_alloc;
		}

		public:

			typedef Object_pool<Rm_client> Pool;

			static Pool * pool()
			{
				static Pool _pool;
				return &_pool;
			}

			Genode::Thread_capability const thread;

			Rm_client(Genode::Thread_capability t) :
				Object_pool<Rm_client>::Entry(_id_alloc()->alloc()), thread(t)
			{ }
	};

	/**
	 * Handles faults on eavesdropping RM sessions
	 */
	class Fault_handler : public Genode::Thread<8*1024>
	{
		Signal_context _fault;
		Signal_receiver _src;
		Signal_context_capability const _cap;
		Genode::Signal_transmitter _dst;
		Genode::Lock _dst_lock;
		Genode::Rm_session_client * const _rm;

		public:

			Fault_handler(Genode::Rm_session_client * const rm, Signal_context_capability dst = Signal_context_capability()) :
				_cap(_src.manage(&_fault)), _dst(dst), _rm(rm)
			{ }

			void dst(Signal_context_capability dst)
			{
				Genode::Lock::Guard guard(_dst_lock);
				_dst.context(dst);
			}

			Signal_context_capability cap() const { return _cap; }

			void entry()
			{
				while (1) {
					Genode::Signal s = _src.wait_for_signal();
					Genode::Rm_session::State state = _rm->state();
					PERR("--- Fault handler %p: Client %u faulted ---", this, state.imprint);
					Genode::Lock::Guard guard(_dst_lock);
					_dst.submit(s.num());
				}
			}
	};

	/**
	 * Session component of a eavesdropping RM service
	 */
	class Rm_session_component : public Genode::Rpc_object<Genode::Rm_session>
	{
		Genode::Rm_session_client _parent_rm;
		Allocator * const _md_alloc;

		public:

			Rm_session_component(const char * args, Allocator * md) :
				_parent_rm(Genode::env()->parent()->session<Rm_session>(args)),
				_md_alloc(md)
			{ }

			~Rm_session_component() { }

			/****************
			 ** Rm_session **
			 ****************/

			void processed(State state) { _parent_rm.processed(state); }

			Local_addr attach(Genode::Dataspace_capability ds,
			                  size_t s, Genode::off_t o,
			                  bool use_la, Genode::Rm_session::Local_addr la,
			                  bool x)
			{
				return _parent_rm.attach(ds, s, o, use_la, la, x);
			}

			void detach(Local_addr la) { _parent_rm.detach(la); }

			Genode::Pager_capability add_client(Genode::Thread_capability t,
			                                    unsigned)
			{
				Rm_client * const c = new (_md_alloc) Rm_client(t);
				Rm_client::pool()->insert(c);
				return _parent_rm.add_client(t, c->id());
			}

			void fault_handler(Signal_context_capability handler) {
				_parent_rm.fault_handler(handler); }

			State state() { return _parent_rm.state(); }

			Genode::Dataspace_capability dataspace() {
				return _parent_rm.dataspace(); }
	};

	/**
	 * Root component of a eavesdropping RM service
	 */
	class Rm_root : public Genode::Root_component<Rm_session_component>
	{
		protected:

			Rm_session_component *_create_session(const char *args)
			{
				/* XXX This is not applied to the childs quota yet XXX */
				Rm_session_component * session =
					new (md_alloc()) Rm_session_component(args, md_alloc());
				return session;
			}

		public:

			Rm_root(Rpc_entrypoint * ep, Allocator * md) :
				Genode::Root_component<Rm_session_component>(ep, md)
			{ }
	};

	/**
	 * Connection to a local eavesdropping RM service
	 */
	class Rm_connection
	{
		enum { FORMAT_STRING_SIZE = Genode::Parent::Session_args::MAX_SIZE };

		Rm_root * _root;
		Genode::Capability<Genode::Rm_session> _cap;

		public:

			Rm_connection(Rm_root * root, void * base, size_t size):
				_root(root),
				_cap(session("ram_quota=64K, start=0x%p, size=0x%x", base, size))
			{ }

			~Rm_connection() { _root->close(_cap); }

			Genode::Rm_session_capability cap() const { return _cap; }

			Genode::Rm_session_capability session(const char *format_args, ...)
			{
				char buf[FORMAT_STRING_SIZE];
				va_list list;
				va_start(list, format_args);
				Genode::String_console sc(buf, FORMAT_STRING_SIZE);
				sc.vprintf(format_args, list);
				va_end(list);
				return Genode::reinterpret_cap_cast<Genode::Rm_session>(_root->session(buf));
			}
	};

	class Child_registry;

	/**
	 * Child that might use virtualized resources
	 */
	class Child : Genode::Child_policy
	{
		private:

			friend class Child_registry;

			Genode::List_element<Child> _list_element;

			Xml_node _start_node;

			Xml_node _default_route_node;

			Name_registry *_name_registry;

			/**
			 * Unique child name and file name of ELF binary
			 */
			struct Name
			{
				enum { MAX_NAME_LEN = 64 };
				char file[MAX_NAME_LEN];
				char unique[MAX_NAME_LEN];

				/**
				 * Constructor
				 *
				 * Obtains file name and unique process name from XML node
				 *
				 * \param start_node XML start node
				 * \param registry   registry tracking unique names
				 */
				Name(Xml_node start_node, Name_registry const *registry) {
					try {
						start_node.attribute("name").value(unique, sizeof(unique)); }
					catch (Xml_node::Nonexistent_attribute) {
						PWRN("Missing 'name' attribute in '<start>' entry.\n");
						throw; }

					/* check for a name confict with the other children */
					if (!registry->is_unique(unique)) {
						PERR("Child name \"%s\" is not unique", unique);
						class Child_name_is_not_unique { };
						throw Child_name_is_not_unique();
					}

					/* use name as default file name if not declared otherwise */
					Genode::strncpy(file, unique, sizeof(file));

					/* check for a binary declaration */
					try {
						Xml_node binary = start_node.sub_node("binary");
						binary.attribute("name").value(file, sizeof(file));
					} catch (...) { }
				}
			} _name;

			/**
			 * Resources assigned to the child
			 */
			struct Resources
			{
				long                   prio_levels_log2;
				long                   priority;
				size_t         ram_quota;
				Ram_connection ram;
				Cpu_connection cpu;
				Rm_connection rm;

				enum { RM_BASE = ~0UL, RM_SIZE = 0 };

				Resources(Xml_node start_node, const char *label,
				          long prio_levels_log2, Rm_root * rm_root)
				:
					prio_levels_log2(prio_levels_log2),
					priority(read_priority(start_node)),
					ram_quota(read_ram_quota(start_node)),
					ram(label),
					cpu(label, priority*(Cpu_session::PRIORITY_LIMIT >> prio_levels_log2)),
					rm(rm_root, (void *)RM_BASE, RM_SIZE)
				{
					/* deduce session costs from usable ram quota */
					size_t session_donations = Genode::Rm_connection::RAM_QUOTA +
					                                   Cpu_connection::RAM_QUOTA +
					                                   Ram_connection::RAM_QUOTA;

					if (ram_quota > session_donations)
						ram_quota -= session_donations;
					else ram_quota = 0;

					ram.ref_account(Genode::env()->ram_session_cap());
					Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota);
				}
			} _resources;

			/*
			 * Entry point used for serving the parent interface and the
			 * locally provided ROM sessions for the 'config' and 'binary'
			 * files.
			 */
			enum { ENTRYPOINT_STACK_SIZE = 12*1024 };
			Rpc_entrypoint _entrypoint;

			/**
			 * ELF binary
			 */
			Genode::Rom_connection _binary_rom;

			/**
			 * Private child configuration
			 */
			Init::Child_config _config;

			/**
			 * Each child of init can act as a server
			 */
			Server _server;

			Genode::Child _child;

			Service_registry *_local_services;
			Service_registry *_parent_services;
			Service_registry *_child_services;

			/**
			 * Policy helpers
			 */
			Init::Child_policy_enforce_labeling      _labeling_policy;
			Init::Child_policy_handle_cpu_priorities _priority_policy;
			Init::Child_policy_provide_rom_file      _config_policy;
			Init::Child_policy_provide_rom_file      _binary_policy;
			Init::Child_policy_redirect_rom_file     _configfile_policy;

		public:

			Child(Xml_node          start_node,
			      Xml_node          default_route_node,
			      Name_registry            *name_registry,
			      long                      prio_levels_log2,
			      Service_registry *local_services,
			      Service_registry *parent_services,
			      Service_registry *child_services,
			      Genode::Cap_session      *cap_session,
			      Rm_root * rm_root)
			:
				_list_element(this),
				_start_node(start_node),
				_default_route_node(default_route_node),
				_name_registry(name_registry),
				_name(start_node, name_registry),
				_resources(start_node, _name.unique, prio_levels_log2, rm_root),
				_entrypoint(cap_session, ENTRYPOINT_STACK_SIZE, _name.unique, false),
				_binary_rom(_name.file, _name.unique),
				_config(_resources.ram.cap(), start_node),
				_server(_resources.ram.cap()),
				_child(_binary_rom.dataspace(), _resources.ram.cap(),
				       _resources.cpu.cap(), _resources.rm.cap(), &_entrypoint, this),
				_local_services(local_services),
				_parent_services(parent_services),
				_child_services(child_services),
				_labeling_policy(_name.unique),
				_priority_policy(_resources.prio_levels_log2, _resources.priority),
				_config_policy("config", _config.dataspace(), &_entrypoint),
				_binary_policy("binary", _binary_rom.dataspace(), &_entrypoint),
				_configfile_policy("config", _config.filename())
			{
				using namespace Genode;

				if (_resources.ram_quota == 0)
					PWRN("no valid RAM resource for child \"%s\"", _name.unique);

				if (config_verbose) {
					Genode::printf("child \"%s\"\n", _name.unique);
					Genode::printf("  RAM quota:  %zd\n", _resources.ram_quota);
					Genode::printf("  ELF binary: %s\n", _name.file);
					Genode::printf("  priority:   %ld\n", _resources.priority);
				}

				/*
				 * Determine services provided by the child
				 */
				try {
					Xml_node service_node = start_node.sub_node("provides").sub_node("service");

					for (; ; service_node = service_node.next("service")) {

						char name[Service::MAX_NAME_LEN];
						service_node.attribute("name").value(name, sizeof(name));

						if (config_verbose)
							Genode::printf("  provides service %s\n", name);

						child_services->insert(new (_child.heap())
							Routed_service(name, &_server));

					}
				} catch (Xml_node::Nonexistent_sub_node) { }
			}

			virtual ~Child() { }

			/**
			 * Return true if the child has the specified name
			 */
			bool has_name(const char *n) const { return !Genode::strcmp(name(), n); }

			Server *server() { return &_server; }

			/**
			 * Start execution of child
			 */
			void start() { _entrypoint.activate(); }


			/****************************
			 ** Child-policy interface **
			 ****************************/

			const char *name() const { return _name.unique; }

			Service *resolve_session_request(const char *service_name,
			                                         const char *args)
			{
				Service *service = 0;

				/* check for config file request */
				if ((service = _config_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for binary file request */
				if ((service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				service = _local_services->find(service_name);
				if (service) return service;

				try {
					Xml_node route_node = _default_route_node;
					try {
						route_node = _start_node.sub_node("route"); }
					catch (...) { }
					Xml_node service_node = route_node.sub_node();

					for (; ; service_node = service_node.next()) {

						bool service_wildcard = service_node.has_type("any-service");

						if (!service_node_matches(service_node, service_name))
							continue;

						if (!service_node_args_condition_satisfied(service_node, args))
							continue;

						Xml_node target = service_node.sub_node();
						for (; ; target = target.next()) {

							if (target.has_type("parent")) {
								service = _parent_services->find(service_name);
								if (service)
									return service;

								if (!service_wildcard) {
									PWRN("%s: service lookup for \"%s\" at parent failed", name(), service_name);
									return 0;
								}
							}

							if (target.has_type("child")) {
								char server_name[Name::MAX_NAME_LEN];
								server_name[0] = 0;
								target.attribute("name").value(server_name, sizeof(server_name));

								Server *server = _name_registry->lookup_server(server_name);
								if (!server)
									PWRN("%s: invalid route to non-existing server \"%s\"", name(), server_name);

								service = _child_services->find(service_name, server);
								if (service)
									return service;

								if (!service_wildcard) {
									PWRN("%s: lookup to child service \"%s\" failed", name(), service_name);
									return 0;
								}
							}

							if (target.has_type("any-child")) {
								if (_child_services->is_ambiguous(service_name)) {
									PERR("%s: ambiguous routes to service \"%s\"", name(), service_name);
									return 0;
								}
								service = _child_services->find(service_name);
								if (service)
									return service;

								if (!service_wildcard) {
									PWRN("%s: lookup for service \"%s\" failed", name(), service_name);
									return 0;
								}
							}

							if (target.is_last())
								break;
						}
					}
				} catch (...) {
					PWRN("%s: no route to service \"%s\"", name(), service_name);
				}
				return service;
			}

			void filter_session_args(const char *service,
			                         char *args, size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
				_priority_policy.filter_session_args(service, args, args_len);
				_configfile_policy.filter_session_args(service, args, args_len);
			}

			bool announce_service(const char             *service_name,
			                      Root_capability root,
			                      Allocator      *alloc)
			{
				if (config_verbose)
					Genode::printf("child \"%s\" announces service \"%s\"\n",
					               name(), service_name);

				Service *s = _child_services->find(service_name, &_server);
				Routed_service *rs = dynamic_cast<Routed_service *>(s);
				if (!s || !rs) {
					PERR("%s: illegal announcement of service \"%s\"", name(), service_name);
					return false;
				}

				rs->announce(root);
				return true;
			}
	};

	typedef Genode::List<Genode::List_element<Child> > Child_list;

	class Child_registry : public Name_registry, Child_list
	{
		public:

			/**
			 * Register child
			 */
			void insert(Child *child)
			{
				Child_list::insert(&child->_list_element);
			}

			/**
			 * Start execution of all children
			 */
			void start() {
				Genode::List_element<Child> *curr = first();
				for (; curr; curr = curr->next())
					curr->object()->start();
			}


			/*****************************
			 ** Name-registry interface **
			 *****************************/

			bool is_unique(const char *name) const
			{
				Genode::List_element<Child> *curr = first();
				for (; curr; curr = curr->next())
					if (curr->object()->has_name(name))
						return false;

				return true;
			}

			Server *lookup_server(const char *name) const
			{
				Genode::List_element<Child> *curr = first();
				for (; curr; curr = curr->next())
					if (curr->object()->has_name(name))
						return curr->object()->server();

				return 0;
			}
	};

	/**
	 * Child that emulates an adder through the emulator service
	 */
	class Emulator_child : public Genode::Child_policy,
	                       public Init::Child_policy_enforce_labeling
	{
		enum { STACK_SIZE = 8*1024 };

		/**
		 * Wrapper for the childs raw ressources
		 */
		struct Resources
		{
			Ram_connection ram;
			Cpu_connection cpu;
			Genode::Rm_connection  rm;

			/* Session costs to be deduced from usable ram quota */
			enum { DONATIONS = Genode::Rm_connection::RAM_QUOTA +
							   Cpu_connection::RAM_QUOTA +
							   Ram_connection::RAM_QUOTA };

			/**
			 * Constructor
			 *
			 * \param  label  Label for 'ram' and 'cpu'
			 */
			Resources(char const *label, size_t ram_quota) :
				ram(label), cpu(label)
			{
				if (ram_quota > DONATIONS) ram_quota -= DONATIONS;
				else ram_quota = 0;
				ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota);
			}

		} _resources;

		const char * _name; /* name of the emulator process */
		Rpc_entrypoint _entrypoint; /* to serve the parent interface */
		Root_capability _root; /* root of the emulator service */
		Genode::Lock _ready; /* to block till emulator service is available */
		Genode::Child _child; /* emulator process and parent interface */
		Service_registry _parent_services; /* services that shall be
		                                            * routed to our parent */

		public:

			typedef Id_allocator<MAX_EMULATORS> Id_alloc;

			/**
			 * To assign unique IDs to emulators
			 */
			static Id_alloc * id_alloc()
			{
				static Id_alloc _id_alloc;
				return &_id_alloc;
			}

			/**
			 * Constructor
			 */
			Emulator_child(const char * name,
						  Genode::Dataspace_capability elf_ds,
						  size_t const ram_quota,
						  Genode::Cap_session * const cap_session)
			:
				Init::Child_policy_enforce_labeling(name),
				_resources(name, ram_quota),
				_name(name),
				_entrypoint(cap_session, STACK_SIZE, name, false),
				_ready(Genode::Lock::LOCKED),
				_child(elf_ds, _resources.ram.cap(), _resources.cpu.cap(),
					   _resources.rm.cap(), &_entrypoint, this)
			{
				_entrypoint.activate();
				_ready.lock();
			}

			/***************
			 ** Accessors **
			 ***************/

			Root_capability root() { return _root; }

			/**************************
			 ** Genode::Child_policy **
			 **************************/

			bool announce_service(const char * name, Root_capability r,
			                      Allocator * const a, Server *)
			{
				if (Genode::strcmp(name, "Emulator")) return 0;
				_root = r;
				_ready.unlock();
				return 1;
			}

			const char * name() const { return _name; }

			Service * resolve_session_request(const char * service,
			                                          const char * args)
			{
				using namespace Genode;
				Service * s = _parent_services.find(service);
				if (!s) {
					s = new (env()->heap()) Parent_service(service);
					_parent_services.insert(s);
				}
				return s;
			}

			void filter_session_args(const char * service, char * args,
			                         size_t const s)
			{
				using namespace Init;
				Child_policy_enforce_labeling::filter_session_args(0, args, s);
			}
	};

	/**
	 * Fault handler for user MMIO that corresponds to an emulated adder
	 */
	class Io_mem_fault_handler : public Genode::Thread<8*1024>
	{
		Genode::Rm_session * const _rm; /* Nested RM wich backs the user IO_MEM */
		Signal_receiver * const _sig_recvr; /* Receives fault signals */
		Emulator::Session_client * const _emu; /* Session to emulate
		                                        * sideeffects of the faults */

		public:

			/**
			 * Constructor
			 */
			Io_mem_fault_handler(Genode::Rm_session * const rm,
			                     Signal_receiver * const sr,
			                     Emulator::Session_client * const emu)
			:
				_rm(rm), _sig_recvr(sr), _emu(emu)
			{ }

			/**
			 * Process one pending fault
			 */
			void handle_fault()
			{
				/* Fetch fault attributes */
				using namespace Genode;
				Rm_session::State s = _rm->state();
				if (s.type == Rm_session::READY) {
					PWRN("Ignoring spurious fault signal");
					return;
				}
				/* Emulate instruction through our emulator */
				switch(s.type)
				{
				case Rm_session::WRITE_FAULT: {
					_emu->write_mmio(s.addr, Emulator::Session::LSB32, s.value);
					printf("Write to MMIO 0x%lx: 0x%lx\n", s.addr, s.value);
					break; }
				case Rm_session::READ_FAULT: {
					s.value = _emu->read_mmio(s.addr, Emulator::Session::LSB32);
					printf("Read from MMIO 0x%lx: 0x%lx\n", s.addr, s.value);
					break; }
				default: {
					PERR("%s:%d: Invalid fault type", __FILE__, __LINE__);
					return; }
				}
				/* End fault */
				_rm->processed(s);
			}

			/**
			 * Thread main routine
			 */
			void entry()
			{
				while (1) {
					Genode::Signal s = _sig_recvr->wait_for_signal();
					for (int i = 0; i < s.num(); i++) handle_fault();
				}
			}
	};

	/**
	 * Session component of an IO_MEM service for emulated adder MMIO
	 */
	class Io_mem_session_component : public Genode::Rpc_object<Genode::Io_mem_session>
	{
		enum {
			EMULATOR_RAM = 512*1024,
			EMULATOR_SESSION_RAM = 8*1024
		};

		/* To receive pagefaults from the user RM-session */
		Genode::Rm_connection _rm;
		Signal_receiver _fault_recvr;
		Signal_context _fault;
		Io_mem_fault_handler * _fault_handler;

		/* To start an emulator child */
		Cap_connection _emu_cap;
		Emulator_child * _emu;
		Genode::Rom_connection * _emu_elf;
		Genode::Dataspace_capability _emu_elf_ds;

		/* To create a session to the service of our emulator child */
		Genode::Root_client * _emu_root;
		Emulator::Session_client * _emu_session;

		public:

			/**
			 * Constructor
			 */
			Io_mem_session_component(Rm_root * const rm_root,
			                         addr_t const base,
			                         size_t const s) :
				_rm(0, 0x1000)
			{
				/* get the emulator binary */
				using namespace Genode;
				enum { MAX_EMU_ELF_NAME_LENGTH = 32,
				       MAX_EMU_NAME_LENGTH = 32 };
				char const elf_name[MAX_EMU_ELF_NAME_LENGTH] = "adder32";
				try {
					_emu_elf = new (env()->heap()) Rom_connection(elf_name);
					_emu_elf_ds = _emu_elf->dataspace();
				} catch (...) {
					PERR("%s:%d: Failed to load binary", __FILE__, __LINE__);
					while (1);
				}
				/* start an emulator child */
				unsigned const id = Emulator_child::id_alloc()->alloc();
				char name[MAX_EMU_NAME_LENGTH];
				Genode::snprintf(name, sizeof(name), "%s_%i", elf_name, id);
				_emu = new (env()->heap())
					Emulator_child(name, _emu_elf_ds, EMULATOR_RAM, &_emu_cap);

				/* create a session to the service of our emulator child */
				_emu_root = new (env()->heap()) Root_client(_emu->root());
				char args[32];
				snprintf(args, sizeof(args), "ram_quota=%i",
				         EMULATOR_SESSION_RAM);
				Emulator::Session_capability c;
				c = static_cap_cast<Emulator::Session>(_emu_root->session(args));
				_emu_session = new (env()->heap()) Emulator::Session_client(c);

				/* set fault handler and start handling */
				_fault_handler = new (env()->heap())
					Io_mem_fault_handler(&_rm, &_fault_recvr, _emu_session);
				_rm.fault_handler(_fault_recvr.manage(&_fault));
				_fault_handler->start();
			}

			/**
			 * Get emulated IO_MEM concealed as normal IO_MEM dataspace
			 */
			Genode::Io_mem_dataspace_capability dataspace()
			{
				using namespace Genode;
				return static_cap_cast<Io_mem_dataspace>(_rm.dataspace());
			}
	};

	/**
	 * Root component of an IO_MEM service for emulated MMIO
	 */
	class Io_mem_root : public Genode::Root_component<Io_mem_session_component>
	{
		Rm_root * const _rm_root;

		/**
		 * Create a new session for the requested MMIO
		 *
		 * \param  args  Session-creation arguments
		 */
		Io_mem_session_component * _create_session(const char * args)
		{
			using namespace Genode;
			addr_t b = Arg_string::find_arg(args, "base").ulong_value(0);
			size_t s = Arg_string::find_arg(args, "size").ulong_value(0);
			return new (md_alloc()) Io_mem_session_component(_rm_root, b, s);
		}

		public:

			/**
			 * Constructor
			 *
			 * \param  ep        Entrypoint for the root component
			 * \param  md_alloc  Meta-data allocator for the root component
			 */
			Io_mem_root(Rpc_entrypoint * const ep,
			            Allocator * const md,
			            Rm_root * const rm_root)
			:
				Genode::Root_component<Io_mem_session_component>(ep, md),
				_rm_root(rm_root)
			{ }
	};


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


	template <typename SESSION_TYPE>
	class Local_service : public Service
	{
		Genode::Root_component<SESSION_TYPE> * const _root;

		public:

			Local_service(Genode::Root_component<SESSION_TYPE> * const root) :
				Service(SESSION_TYPE::service_name()), _root(root)
			{ }

			Session_capability session(const char *args) {
				return _root->session(args); }

			void upgrade(Session_capability, const char *)
			{
				PERR("%s:%d: Not implemented", __FILE__, __LINE__);
				while (1) ;
			}

			void close(Session_capability session) {
				_root->close(session); }
	};
}

int main(int, char **)
{
	using namespace Init;

	try {
		config_verbose =
			Genode::config()->xml_node().attribute("verbose").has_value("yes"); }
	catch (...) { }

	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	enum { ENTRYPOINT_STACK_SIZE = 8*1024 };

	static Sliced_heap sliced_heap(Genode::env()->ram_session(), Genode::env()->rm_session());
	static Cap_connection   cap;
	static Rpc_entrypoint   ep(&cap, ENTRYPOINT_STACK_SIZE, "entrypoint");
	static Service_registry local_services;
	static Service_registry parent_services;
	static Service_registry child_services;
	static Child_registry           children;

	/* provide eavesdropping RM service locally */
	static Rm_root rm_root(&ep, &sliced_heap);
	static Local_service<Rm_session_component> rm(&rm_root);
	local_services.insert(&rm);

	/* provide virtualized IO MEM service locally */
	static Io_mem_root io_mem_root(&ep, &sliced_heap, &rm_root);
	static Local_service<Io_mem_session_component> io_mem(&io_mem_root);
	local_services.insert(&io_mem);

	try { determine_parent_services(&parent_services); }
	catch (...) { }

	/* determine default route for resolving service requests */
	Xml_node default_route_node("<empty/>");
	try {
		default_route_node =
			Genode::config()->xml_node().sub_node("default-route"); }
	catch (...) { }

	/* create children */
	try {
		Xml_node start_node = Genode::config()->xml_node().sub_node("start");
		for (;; start_node = start_node.next("start")) {

			children.insert(new (Genode::env()->heap())
							Init::Child(start_node, default_route_node, &children,
								  read_prio_levels_log2(), &local_services,
								  &parent_services, &child_services, &cap, &rm_root));

			if (start_node.is_last("start")) break;
		}
	}
	catch (Xml_node::Nonexistent_sub_node) {
		PERR("No children to start");
	}

	/* start children */
	children.start();

	Genode::sleep_forever();
	return 0;
}

