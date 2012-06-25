/*
 * \brief  Singlethreaded kernel
 * \author Martin stein
 * \date   2011-10-20
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/signal.h>
#include <base/vm_state.h>
#include <util/fifo.h>
#include <util/avl_tree.h>

/* Core includes */
#include <kernel_support.h>
#include <platform_thread.h>
#include <assert.h>
#include <software_tlb.h>

using namespace Kernel;

/* Get core configuration */
extern Genode::addr_t _call_after_kernel;
extern Genode::Native_utcb * _main_utcb;
extern int _kernel_stack_high;
extern "C" void CORE_MAIN();

/* Get structure of mode transition PIC */
extern int _mode_transition_begin;
extern int _mode_transition_end;
extern int _mt_user_entry_pic;
extern int _mt_kernel_entry_pic;
extern Genode::addr_t _mt_user_context_ptr;
extern Genode::addr_t _mt_kernel_context_begin;
extern Genode::addr_t _mt_kernel_context_end;

namespace Kernel
{
	/* Import Genode types */
	typedef Genode::size_t size_t;
	typedef Genode::addr_t addr_t;
	typedef Genode::umword_t umword_t;
	typedef Genode::Signal Signal;
	typedef Genode::Pagefault Pagefault;
	typedef Genode::Native_utcb Native_utcb;
	typedef Genode::Platform_thread Platform_thread;
	template <typename T> class Fifo : public Genode::Fifo<T> { };
	template <typename T> class Avl_node : public Genode::Avl_node<T> { };
	template <typename T> class Avl_tree : public Genode::Avl_tree<T> { };

	enum
	{
		/* Kernel coniguration */
		DEFAULT_STACK_SIZE = 1*1024*1024,
		USER_TIME_SLICE_MS = 10,
		MAX_PDS = 256,
		MAX_THREADS = 256,
		MAX_SIGNAL_RECEIVERS = 256,
		MAX_SIGNAL_CONTEXTS = 256,
	};

	/**
	 * Double connected list
	 *
	 * \param  ENTRY_T  List entry type
	 */
	template <typename ENTRY_T>
	class Double_list
	{
		private:

			ENTRY_T * _head;
			ENTRY_T * _tail;

		public:

			class Entry
			{
				friend class Double_list<ENTRY_T>;

				private:

					ENTRY_T * _next;
					ENTRY_T * _prev;
					Double_list<ENTRY_T> * _list;

				public:

					Entry(): _next(0), _prev(0), _list(0) { }

					/***************
					 ** Accessors **
					 ***************/

					ENTRY_T * next() const { return _next; }
					ENTRY_T * prev() const { return _prev; }
			};

		public:

			/**
			 * Constructor
			 *
			 * Start with an empty list.
			 */
			Double_list(): _head(0), _tail(0) { }

			/**
			 * Insert entry from behind into list
			 */
			void insert_tail(ENTRY_T * const e)
			{
				/* Avoid leaking lists */
				if (e->Entry::_list)
					e->Entry::_list->remove(e);

				/* Update new entry */
				e->Entry::_prev = _tail;
				e->Entry::_next = 0;
				e->Entry::_list = this;

				/* Update previous entry or _head */
				if (_tail) _tail->Entry::_next = e;  /* List was not empty */
				else       _head = e;                /* List was empty */
				           _tail = e;
			}

			/**
			 * Remove specific entry from list
			 */
			void remove(ENTRY_T * const e)
			{
				/* Sanity checks */
				if (!_head || e->Entry::_list != this) return;

				/* Update next entry or _tail */
				if (e != _tail) e->Entry::_next->Entry::_prev = e->Entry::_prev;
				else _tail = e->Entry::_prev;

				/* Update previous entry or _head */
				if (e != _head) e->Entry::_prev->Entry::_next = e->Entry::_next;
				else _head = e->Entry::_next;

				/* Update removed entry */
				e->Entry::_list = 0;
			}

			/**
			 * Remove head from list and return it
			 */
			ENTRY_T * remove_head()
			{
				/* Sanity checks */
				if (!_head) return 0;

				/* Update _head */
				ENTRY_T * const e = _head;
				_head = e->Entry::_next;

				/* Update next entry or _tail */
				if (_head) _head->Entry::_prev = 0;
				else _tail = 0;

				/* Update removed entry */
				e->Entry::_list = 0;
				return e;
			}

			/**
			 * Remove head from list and insert it at the end
			 */
			void head_to_tail()
			{
				/* Sanity checks */
				if (!_head || _head == _tail) return;

				/* Remove entry */
				ENTRY_T * const e = _head;
				_head = _head->Entry::_next;
				e->Entry::_next = 0;
				_head->Entry::_prev = 0;

				/* Insert entry */
				_tail->Entry::_next = e;
				e->Entry::_prev = _tail;
				_tail = e;
			}

			/***************
			 ** Accessors **
			 ***************/

			ENTRY_T * head() const { return _head; }
			ENTRY_T * tail() const { return _tail; }
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
			class Entry : public Avl_node<Entry>
			{
				protected:

					unsigned long _id;

				public:

					/**
					 * Constructors
					 */
					Entry(unsigned long const id) : _id(id) { }

					/**
					 * Find entry with 'object_id' within this AVL subtree
					 */
					Entry * find(unsigned long const object_id)
					{
						if (object_id == id()) return this;

						Entry * const subtree = child(object_id > id());

						return subtree ? subtree->find(object_id) : 0;
					}

					/**
					 * ID of this object
					 */
					unsigned long const id() const { return _id; }

					/************************
					 * 'Avl_node' interface *
					 ************************/

					bool higher(Entry *e) { return e->id() > id(); }
			};

		private:

			Avl_tree<Entry> _tree;

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
			Object * object(unsigned long const id)
			{
				Entry * object = _tree.first();
				return (Object *)(object ? object->find(id) : 0);
			}
	};

	/**
	 * Copy 'size' successive bytes from 'src_base' to 'dst_base'
	 */
	inline void copy_range(void * const src_base, void * const dst_base,
	                       size_t size)
	{
		for (unsigned long off = 0; off < size; off += sizeof(umword_t))
			*(umword_t *)((addr_t)dst_base + off) =
				*(umword_t *)((addr_t)src_base + off);
	}

	/**
	 * Sends requests to other IPC nodes, accumulates request announcments,
	 * provides serial access to them and replies to them if expected.
	 * Synchronizes communication.
	 *
	 * IPC node states:
	 *
	 *         +----------+                               +---------------+                             +---------------+
	 * --new-->| inactive |--send-request-await-reply---->| await reply   |               +--send-note--| prepare reply |
	 *         |          |<--receive-reply---------------|               |               |             |               |
	 *         |          |                               +---------------+               +------------>|               |
	 *         |          |<--request-is-a-note-------+---request-is-not-a-note------------------------>|               |
	 *         |          |                           |   +---------------+                             |               |
	 *         |          |--await-request------------+-->| await request |<--send-reply-await-request--|               |
	 *         |          |--send-reply-await-request-+-->|               |--announce-request-+-------->|               |
	 *         |          |--send-note--+             |   +---------------+                   |         |               |
	 *         |          |             |      request|available                              |         |               |
	 *         |          |<------------+             |                                       |         |               |
	 *         |          |<--request-is-a-note-------+---request-is-not-a-note---------------|-------->|               |
	 *         |          |<--request-is-a-note-----------------------------------------------+         |               |
	 *         +----------+                 +-------------------------+                                 |               |
	 *                                      | prepare and await reply |<--send-request-and-await-reply--|               |
	 *                                      |                         |--receive-reply----------------->|               |
	 *                                      +-------------------------+                                 +---------------+
	 *
	 * State model propagated to deriving classes:
	 *
	 *         +--------------+                               +----------------+
	 * --new-->| has received |--send-request-await-reply---->| awaits receipt |
	 *         |              |--await-request------------+-->|                |
	 *         |              |                           |   |                |
	 *         |              |<--request-available-------+   |                |
	 *         |              |--send-reply-await-request-+-->|                |
	 *         |              |--send-note--+             |   |                |
	 *         |              |             |             |   |                |
	 *         |              |<------------+             |   |                |
	 *         |              |<--request-available-------+   |                |
	 *         |              |<--announce-request------------|                |
	 *         |              |<--receive-reply---------------|                |
	 *         +--------------+                               +----------------+
	 */
	class Ipc_node
	{
		/* IPC node states as depicted initially */
		enum State
		{
			INACTIVE = 1,
			AWAIT_REPLY = 2,
			AWAIT_REQUEST = 3,
			PREPARE_REPLY = 4,
			PREPARE_AND_AWAIT_REPLY = 5,
		};

		/**
		 * Describes the buffer for incoming or outgoing messages
		 */
		struct Message_buf : public Fifo<Message_buf>::Element
		{
			void * base;
			size_t size;
			Ipc_node * origin;
		};

		Fifo<Message_buf> _request_queue; /* requests that waits to be
		                                   * received by us */
		Message_buf _inbuf; /* buffers message we have received lastly */
		Message_buf _outbuf; /* buffers the message we aim to send */
		State _state; /* Current node state */

		/**
		 * Buffer next request from request queue to handle it
		 */
		inline void _receive_request(Message_buf * const r)
		{
			/* Assertions */
			assert(r->size <= _inbuf.size);

			/* Fetch message */
			copy_range(r->base, _inbuf.base, r->size);
			_inbuf.size = r->size;
			_inbuf.origin = r->origin;

			/* Update state */
			_state = r->origin->_awaits_reply() ? PREPARE_REPLY :
			                                      INACTIVE;
		}

		/**
		 * Receive a given reply if one is expected
		 *
		 * \param  base  Base of the reply payload
		 * \param  size  Size of the reply payload
		 */
		inline void _receive_reply(void * const base, size_t const size)
		{
			/* Assertions */
			assert(_awaits_reply());
			assert(size <= _inbuf.size);

			/* Receive reply */
			copy_range(base, _inbuf.base, size);
			_inbuf.size = size;

			/* Update state */
			if (_state != PREPARE_AND_AWAIT_REPLY) _state = INACTIVE;
			else _state = PREPARE_REPLY;
			_has_received(_inbuf.size);
		}

		/**
		 * Insert request 'r' into request queue and buffer it if we
		 * were waiting for it
		 */
		inline void _announce_request(Message_buf * const r)
		{
			/* Directly receive request if we've awaited it */
			if (_state == AWAIT_REQUEST) {
				_receive_request(r);
				_has_received(_inbuf.size);
				return;
			}

			/* Cannot receive yet, so queue request */
			_request_queue.enqueue(r);
		}

		/**
		 * Do we expect to receive a reply message?
		 */
		bool _awaits_reply()
		{
			return _state == AWAIT_REPLY ||
				   _state == PREPARE_AND_AWAIT_REPLY;
		}

		public:

			/**
			 * Constructor
			 */
			inline Ipc_node() : _state(INACTIVE)
			{
				_inbuf.size = 0;
				_outbuf.size = 0;
			}

			/**
			 * Destructor
			 */
			virtual ~Ipc_node() { }

			/**
			 * Send a request and wait for the according reply
			 *
			 * \param  dest        Targeted IPC node
			 * \param  req_base    Base of the request payload
			 * \param  req_size    Size of the request payload
			 * \param  inbuf_base  Base of the reply buffer
			 * \param  inbuf_size  Size of the reply buffer
			 */
			inline void send_request_await_reply(Ipc_node * const dest,
			                                     void * const req_base,
			                                     size_t const req_size,
			                                     void * const inbuf_base,
			                                     size_t const inbuf_size)
			{
				/* Assertions */
				assert(_state == INACTIVE || _state == PREPARE_REPLY);

				/* Prepare transmission of request message */
				_outbuf.base = req_base;
				_outbuf.size = req_size;
				_outbuf.origin = this;

				/* Prepare reception of reply message */
				_inbuf.base = inbuf_base;
				_inbuf.size = inbuf_size;

				/* Update state */
				if (_state != PREPARE_REPLY) _state = AWAIT_REPLY;
				else _state = PREPARE_AND_AWAIT_REPLY;
				_awaits_receipt();

				/* Announce request */
				dest->_announce_request(&_outbuf);
			}

			/**
			 * Wait until a request has arrived and load it for handling
			 *
			 * \param   inbuf_base  Base of the request buffer
			 * \param   inbuf_size  Size of the request buffer
			 */
			inline void await_request(void * const inbuf_base,
			                          size_t const inbuf_size)
			{
				/* Assertions */
				assert(_state == INACTIVE);

				/* Prepare receipt of request */
				_inbuf.base = inbuf_base;
				_inbuf.size = inbuf_size;

				/* If anybody already announced a request receive it */
				if (!_request_queue.empty()) {
					_receive_request(_request_queue.dequeue());
					_has_received(_inbuf.size);
					return;
				}

				/* No request announced, so wait */
				_state = AWAIT_REQUEST;
				_awaits_receipt();
			}

			/**
			 * Reply last request if there's any and await next request
			 *
			 * \param  reply_base  Base of the reply payload
			 * \param  reply_size  Size of the reply payload
			 * \param  inbuf_base  Base of the request buffer
			 * \param  inbuf_size  Size of the request buffer
			 */
			inline void send_reply_await_request(void * const reply_base,
			                                     size_t const reply_size,
			                                     void * const inbuf_base,
			                                     size_t const inbuf_size)
			{
				/* Reply to the last request if we have to */
				if (_state == PREPARE_REPLY) {
					_inbuf.origin->_receive_reply(reply_base, reply_size);
					_state = INACTIVE;
				}

				/* Await next request */
				await_request(inbuf_base, inbuf_size);
			}

			/**
			 * Send a notification and stay inactive
			 *
			 * \param  dest        Targeted IPC node
			 * \param  note_base   Base of the note payload
			 * \param  note_size   Size of the note payload
			 *
			 * The caller must ensure that the note payload remains
			 * until it is buffered by the targeted node.
			 */
			inline void send_note(Ipc_node * const dest,
			                      void * const note_base,
			                      size_t const note_size)
			{
				/* Assertions */
				assert(_state == INACTIVE || _state == PREPARE_REPLY);

				/* Announce request message, our state says: No reply needed */
				_outbuf.base = note_base;
				_outbuf.size = note_size;
				_outbuf.origin = this;
				dest->_announce_request(&_outbuf);
			}

		private:

			/**************************************
			 ** Notification of deriving classes **
			 **************************************/

			/**
			 * IPC node waits for a message to receive to its inbuffer
			 */
			virtual void _awaits_receipt() = 0;

			/**
			 * IPC node has received a message in its inbuffer
			 *
			 * \param  s  Size of the message
			 */
			virtual void _has_received(size_t const s) = 0;
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
		bool _valid_id(unsigned const id) const {
			return id >= MIN && id <= MAX; }

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
	 * Provides kernel object management for 'T'-objects if 'T' derives from it
	 */
	template <typename T, unsigned MAX_INSTANCES>
	class Object : public Object_pool<T>::Entry
	{
		typedef Id_allocator<MAX_INSTANCES> Id_alloc;

		/**
		 * Allocator for unique IDs for all instances of 'T'
		 */
		static Id_alloc * _id_alloc()
		{
			static Id_alloc _id_alloc;
			return &_id_alloc;
		}

		public:

			typedef Object_pool<T> Pool;

			/**
			 * Gets every instance of 'T' by its ID
			 */
			static Pool * pool()
			{
				static Pool _pool;
				return &_pool;
			}

			/**
			 * Placement new
			 *
			 * Kernel objects are normally constructed on a memory
			 * donation so we must be enabled to place them explicitly
			 */
			void * operator new (size_t, void * p) { return p; }

		protected:

			/**
			 * Constructor
			 *
			 * Ensures that we have a unique ID and
			 * can be found through the static object pool
			 */
			Object() : Pool::Entry(_id_alloc()->alloc()) {
				pool()->insert(static_cast<T *>(this)); }
	};

	/**
	 * Provides the mode transition PIC in a configurable and mappable manner
	 *
	 * Initially there exists only the code that switches between kernelmode
	 * and usermode. It must be present in a continuous region that is located
	 * at an arbitray RAM address. Its size must not exceed the smallest page
	 * size supported by the MMU. The Code must be position independent. This
	 * control then duplicates the code to an aligned region and declares a
	 * virtual region, where the latter has to be mapped to in every PD, to
	 * ensure appropriate kernel invokation on CPU interrupts.
	 */
	struct Mode_transition_control
	{
		enum {
			SIZE_LOG2 = Cpu::MIN_PAGE_SIZE_LOG2,
			SIZE = 1 << SIZE_LOG2,
			VIRT_BASE = Cpu::HIGHEST_EXCEPTION_ENTRY,
			VIRT_END = VIRT_BASE + SIZE,
			ALIGNM_LOG2 = SIZE_LOG2,
		};

		/* Writeable, page-aligned backing store */
		char _payload[SIZE] __attribute__((aligned(1 << ALIGNM_LOG2)));

		/* Labels within the aligned mode transition PIC */
		Cpu::Context * * const _user_context_ptr;
		Cpu::Context * const _kernel_context;
		addr_t const _virt_user_entry;
		addr_t const _virt_kernel_entry;

		/**
		 * Constructor
		 */
		Mode_transition_control() :
			_user_context_ptr((Cpu::Context * *)((addr_t)_payload +
			                  ((addr_t)&_mt_user_context_ptr -
			                  (addr_t)&_mode_transition_begin))),

			_kernel_context((Cpu::Context *)((addr_t)_payload +
			                ((addr_t)&_mt_kernel_context_begin -
			                (addr_t)&_mode_transition_begin))),

			_virt_user_entry(VIRT_BASE + ((addr_t)&_mt_user_entry_pic -
			                 (addr_t)&_mode_transition_begin)),

			_virt_kernel_entry(VIRT_BASE + ((addr_t)&_mt_kernel_entry_pic -
			                   (addr_t)&_mode_transition_begin))
		{
			/* Check if mode transition PIC fits into aligned region */
			addr_t const pic_begin = (addr_t)&_mode_transition_begin;
			addr_t const pic_end = (addr_t)&_mode_transition_end;
			size_t const pic_size = pic_end - pic_begin;
			assert(pic_size <= SIZE);

			/* Check if kernel context fits into the mode transition */
			addr_t const kc_begin = (addr_t)&_mt_kernel_context_begin;
			addr_t const kc_end = (addr_t)&_mt_kernel_context_end;
			size_t const kc_size = kc_end - kc_begin;
			assert(sizeof(Cpu::Context) <= kc_size)

			/* Fetch mode transition PIC */
			unsigned * dst = (unsigned *)_payload;
			unsigned * src = (unsigned *)pic_begin;
			while((addr_t)src < pic_end) *dst++ = *src++;

			/* Try to set CPU exception entry accordingly */
			assert(!Cpu::exception_entry_at(_virt_kernel_entry))
		}

		/**
		 * Set next usermode-context pointer
		 */
		void user_context(Cpu::Context * const c) { *_user_context_ptr = c; }

		/**
		 * Fetch next kernelmode context
		 */
		void fetch_kernel_context(Cpu::Context * const c) {
			*_kernel_context = *c; }

		/**
		 * Page aligned physical base of the mode transition PIC
		 */
		addr_t phys_base() { return (addr_t)_payload; }

		/**
		 * Virtual pointer to the usermode entry PIC
		 */
		addr_t virt_user_entry() { return _virt_user_entry; }
	};


	/**
	 * Static mode transition control
	 */
	static Mode_transition_control * mtc() {
		static Mode_transition_control _object; return &_object; }


	/**
	 * Kernel object that represents a Genode PD
	 */
	class Pd : public Object<Pd, MAX_PDS>,
	           public Software_tlb
	{
		/* keep ready memory for size aligned extra costs at construction */
		enum { EXTRA_SPACE_SIZE = 2*Software_tlb::MAX_COSTS_PER_TRANSLATION };
		char _extra_space[EXTRA_SPACE_SIZE];

		public:

			/**
			 * Constructor if ID can be arbitrary
			 */
			Pd()
			{
				/* Try to add translation for mode transition region */
				enum Mtc_attributes { W = 1, X = 1, K = 1, G = 1 };
				unsigned const slog2 = insert_translation(mtc()->VIRT_BASE,
				                                          mtc()->phys_base(),
				                                          mtc()->SIZE_LOG2,
				                                          W, X, K, G);

				/* Extra space needed to translate mode transition region */
				if (slog2)
				{
					/* Get size aligned extra space */
					addr_t const es = (addr_t)&_extra_space;
					addr_t const es_end = es + sizeof(_extra_space);
					addr_t const aligned_es = (es_end - (1<<slog2)) &
					                          ~((1<<slog2)-1);
					addr_t const aligned_es_end = aligned_es + (1<<slog2);

					/* Check attributes of aligned extra space */
					assert(aligned_es >= es && aligned_es_end <= es_end)

					/* Translate mode transition region globally */
					insert_translation(mtc()->VIRT_BASE, mtc()->phys_base(),
					                   mtc()->SIZE_LOG2, W, X, K, G,
					                   (void *)aligned_es);
				}
			}

			/**
			 * Add the CPU context 'c' to this PD
			 */
			void append_context(Cpu::Context * const c)
			{
				c->protection_domain(id());
				c->software_tlb((Software_tlb *)this);
			}
	};

	/**
	 * Simple round robin scheduler for 'ENTRY_T' typed clients
	 */
	template <typename ENTRY_T>
	class Scheduler
	{
		public:

			/**
			 * Base class for 'ENTRY_T' to support scheduling
			 */
			class Entry : public Double_list<ENTRY_T>::Entry
			{
				friend class Scheduler<ENTRY_T>;

				unsigned _time; /* Time wich remains for current lap */

				/**
				 * Apply consumption of 'time'
				 */
				void _consume(unsigned const time) {
					_time = _time > time ? _time - time : 0; }

				public:

					/**
					 * Constructor
					 */
					Entry() : _time(0) { }
			};

		protected:

			ENTRY_T * const _idle; /* Default entry, can't be removed */
			Double_list<ENTRY_T> _entries; /* List of entries beside '_idle' */
			unsigned const _lap_time; /* Time that an entry gets for one
			                           * scheduling lap to consume */

		public:

			/**
			 * Constructor
			 */
			Scheduler(ENTRY_T * const idle, unsigned const lap_time) :
				_idle(idle), _lap_time(lap_time)
			{
				assert(_lap_time && _idle);
			}

			/**
			 * Returns the entry wich shall scheduled next
			 *
			 * \param  t  At the call it contains the time, wich was consumed
			 *            by the last entry. At the return it is updated to
			 *            the next timeslice
			 */
			ENTRY_T * next_entry(unsigned & t)
			{
				/* Update current entry */
				ENTRY_T * e = _entries.head();
				if (!e) {
					t = _idle->Entry::_time;
					return _idle;
				}
				e->Entry::_consume(t);

				/* Lookup entry with time > 0, refresh depleted timeslices */
				while (!e->Entry::_time) {
					e->Entry::_time = _lap_time;
					_entries.head_to_tail();
					e = _entries.head();
				}

				/* Return next entry and appropriate portion of time */
				t = e->Entry::_time;
				return e;
			}

			/**
			 * Get the currently scheduled entry
			 */
			ENTRY_T * current_entry() const {
				return _entries.head() ? _entries.head() : _idle; }

			/**
			 * Ensure that 'e' does participate in scheduling after
			 */
			void insert(ENTRY_T * const e)
			{
				if (e == _idle) return;
				e->Entry::_time = _lap_time;
				_entries.insert_tail(e);
			}

			/**
			 * Ensures that 'e' doesn't participate in scheduling after
			 */
			void remove(ENTRY_T * const e) { _entries.remove(e); }

			/**
			 * Set remaining time of currently scheduled entry to 0
			 */
			void yield()
			{
				ENTRY_T * const e = _entries.head();
				if (e) e->_time = 0;
				return;
			}
	};


	/**
	 * Access to static interrupt-controller
	 */
	static Pic * pic() { static Pic _object; return &_object; }


	/**
	 * Exclusive ownership and handling of one IRQ per instance at a max
	 */
	class Irq_owner : public Object_pool<Irq_owner>::Entry
	{
		/**
		 * To get any instance of this class by its ID
		 */
		typedef Object_pool<Irq_owner> Pool;
		static Pool * _pool() { static Pool _pool; return &_pool; }

		/**
		 * Is called when the IRQ we were waiting for has occured
		 */
		virtual void _received_irq() = 0;

		/**
		 * Is called when we start waiting for the occurence of an IRQ
		 */
		virtual void _awaits_irq() = 0;

		public:

			/**
			 * Translate 'Irq_owner_pool'-entry ID to IRQ ID
			 */
			static unsigned id_to_irq(unsigned id) { return id - 1; }

			/**
			 * Translate IRQ ID to 'Irq_owner_pool'-entry ID
			 */
			static unsigned irq_to_id(unsigned irq) { return irq + 1; }

			/**
			 * Constructor
			 */
			Irq_owner() : Pool::Entry(0) { }

			/**
			 * Destructor
			 */
			virtual ~Irq_owner() { }

			/**
			 * Ensure that our 'receive_irq' gets called on IRQ 'irq'
			 *
			 * \return  0  Couldn't allocate IRQ
			 *          1  IRQ is allocated to us
			 */
			bool allocate_irq(unsigned const irq)
			{
				/* Check if an allocation is needed and possible */
				unsigned const id = irq_to_id(irq);
				if (_id) return _id == id;
				if (_pool()->object(id)) return 0;

				/* Let us own the IRQ, but mask it till we await it */
				pic()->mask(irq);
				_id = id;
				_pool()->insert(this);
				return 1;
			}

			/**
			 * Release the ownership of the IRQ 'irq' if we own it
			 *
			 * \return  0  Couldn't free IRQ
			 *          1  IRQ is freed
			 */
			bool free_irq(unsigned const irq)
			{
				if (_id != irq_to_id(irq)) return 0;
				_pool()->remove(this);
				_id = 0;
				return 1;
			}

			/**
			 * If we own an IRQ, enable it and await 'receive_irq'
			 */
			void await_irq()
			{
				assert(_id);
				unsigned const irq = id_to_irq(_id);
				pic()->unmask(irq);
				_awaits_irq();
			}

			/**
			 * Denote occurence of an IRQ if we own it and awaited it
			 */
			void receive_irq(unsigned const irq)
			{
				assert(_id == irq_to_id(irq));
				pic()->mask(irq);
				_received_irq();
			}

			/**
			 * Get owner of IRQ or 0 if the IRQ is not owned by anyone
			 */
			static Irq_owner * owner(unsigned irq) {
				return _pool()->object(irq_to_id(irq)); }
	};


	/**
	 * Idle thread entry
	 */
	static void idle_main() { while(1); }


	/**
	 * Access to static kernel timer
	 */
	static Timer * timer() { static Timer _object; return &_object; }


	/**
	 * Access to static CPU scheduler
	 */
	class Thread;
	typedef Scheduler<Thread> Cpu_scheduler;
	static Cpu_scheduler * cpu_scheduler();


	/**
	 * Static kernel PD that describes core
	 */
	static Pd * core() { static Pd _object; return &_object; }


	/**
	 * Get core attributes
	 */
	unsigned core_id() { return core()->id(); }


	/**
	 * Kernel object that represents a Genode thread
	 */
	class Thread : public Cpu::User_context,
	               public Object<Thread, MAX_THREADS>,
	               public Cpu_scheduler::Entry,
	               public Ipc_node,
	               public Irq_owner,
	               public Fifo<Thread>::Element
	{
		enum State { STOPPED, ACTIVE, AWAIT_IPC, AWAIT_RESUMPTION,
		             AWAIT_IRQ, AWAIT_SIGNAL };

		Platform_thread * const _platform_thread; /* Userland object wich
		                                           * addresses this thread */
		State _state; /* Thread state, description given at the beginning */
		Pagefault _pagefault; /* Last pagefault triggered
		                               * by this thread */
		Thread * _pager; /* Gets informed via IPC if thread throws
		                         * a pagefault */
		unsigned _pd_id; /* ID of the PD the thread runs on */
		Native_utcb * _phys_utcb; /* Physical UTCB base */
		Native_utcb * _virt_utcb; /* Virtual UTCB base */

		/**
		 * Resume execution of thread
		 */
		void _activate()
		{
			cpu_scheduler()->insert(this);
			_state = ACTIVE;
		}

		public:

			void * operator new (size_t, void * p) { return p; }

			/**
			 * Constructor
			 */
			Thread(Platform_thread * const platform_thread) :
				_platform_thread(platform_thread),
				_state(STOPPED), _pager(0), _pd_id(0),
				_phys_utcb(0), _virt_utcb(0)
			{ }

			/**
			 * Start this thread
			 *
			 * \param ip      instruction pointer to start at
			 * \param sp      stack pointer to use
			 * \param cpu_no  target cpu
			 *
			 * \retval  0  successful
			 * \retval -1  thread could not be started
			 */
			int start(void *ip, void *sp, unsigned cpu_no,
			          unsigned const pd_id, Native_utcb * const phys_utcb,
			          Native_utcb * const virt_utcb);

			/**
			 * Pause this thread
			 */
			void pause()
			{
				/* Check conditions */
				assert(_state == AWAIT_RESUMPTION || _state == ACTIVE);

				cpu_scheduler()->remove(this);
				_state = AWAIT_RESUMPTION;
			}

			/**
			 * Stop this thread
			 */
			void stop()
			{
				cpu_scheduler()->remove(this);
				_state = STOPPED;
			}

			/**
			 * Resume this thread
			 */
			int resume()
			{
				/* Check conditions */
				assert (_state == AWAIT_RESUMPTION || _state == ACTIVE)

				/* Resume thread */
				cpu_scheduler()->insert(this);
				if(_state == ACTIVE) return 1;
				_state = ACTIVE;
				return 0;
			}

			/**
			 * Send a request and await the reply
			 */
			void request_and_wait(Thread * const dest, size_t const size)
			{
				Ipc_node::send_request_await_reply(dest, phys_utcb()->base(),
				                                   size, phys_utcb()->base(),
				                                   phys_utcb()->size());
				/* IPC node triggers thread state change via 'awaits_receipt'
				 * and 'has_received '*/
			}

			/**
			 * Wait for any request
			 */
			void wait_for_request()
			{
				Ipc_node::await_request(phys_utcb()->base(),
				                        phys_utcb()->size());
			}

			/**
			 * Reply to the last request and await the next one
			 */
			void reply_and_wait(size_t const size)
			{
				Ipc_node::send_reply_await_request(phys_utcb()->base(), size,
				                                   phys_utcb()->base(),
				                                   phys_utcb()->size());
			}

			/**
			 * Initialize execution context with program counter 'ip' and
			 * stack pointer 'sp'
			 */
			void init_context(void * const ip, void * const sp,
			                  unsigned const pd_id);

			/**
			 * Handle a pagefault that originates from this thread
			 * \param  va  Virtual fault address
			 * \param  w   Was it a write access?
			 */
			void pagefault(addr_t const va, bool const w);

			/**
			 * Get unique thread ID, avoid method ambiguousness
			 */
			unsigned id() const { return Object::id(); }


			/***************
			 ** Accessors **
			 ***************/

			Platform_thread * platform_thread() const {
				return _platform_thread; }

			void pager(Thread * const p) {
				_pager = p; }

			unsigned pd_id() const { return _pd_id; }

			Native_utcb * phys_utcb() const { return _phys_utcb; }

			/**
			 * Gets called when we await a signal at a signal receiver
			 */
			void await_signal()
			{
				cpu_scheduler()->remove(this);
				_state = AWAIT_IRQ;
			}

			/**
			 * Gets called when we have received a signal at a signal receiver
			 */
			void receive_signal(Signal const s)
			{
				*(Signal *)phys_utcb()->base() = s;
				_activate();
			}

		private:

			/**************
			 ** Ipc_node **
			 **************/

			void _has_received(size_t const s)
			{
				user_arg_0(s);
				if (_state != ACTIVE) _activate();
			}

			void _awaits_receipt()
			{
				cpu_scheduler()->remove(this);
				_state = AWAIT_IPC;
			}

			/***************
			 ** Irq_owner **
			 ***************/

			void _received_irq() { _activate(); }

			void _awaits_irq()
			{
				cpu_scheduler()->remove(this);
				_state = AWAIT_IRQ;
			}
	};

	/**
	 * Specific signal type, owned by a receiver, can be triggered asynchr.
	 */
	class Signal_receiver;
	class Signal_context :
		public Object<Signal_context, MAX_SIGNAL_CONTEXTS>,
		public Fifo<Signal_context>::Element
	{
		friend class Signal_receiver;

		Signal_receiver * const _receiver; /* The receiver that owns us */
		unsigned const _imprint; /* Every of our signals gets signed with */
		unsigned _number; /* How often we got triggered */

		public:

			/**
			 * Constructor
			 */
			Signal_context(Signal_receiver * const r,
			                 unsigned const imprint)
			:
				_receiver(r), _imprint(imprint), _number(0)
			{ }

			/**
			 * Trigger this context
			 *
			 * \param  number  How often this call triggers us at once
			 */
			void trigger_signal(unsigned const number);
	};

	/**
	 * Manage signal contexts and enable threads to trigger and await them
	 */
	class Signal_receiver :
		public Object<Signal_receiver, MAX_SIGNAL_RECEIVERS>
	{
		Fifo<Thread> _listeners;
		Fifo<Signal_context> _pending_contexts;

		/**
		 * Deliver as much submitted signals to listening threads as possible
		 */
		void _listen()
		{
			while (1)
			{
				/* Any pending context? */
				if (_pending_contexts.empty()) return;
				Signal_context * const c = _pending_contexts.dequeue();

				/* If there is no listener, enqueue context again and return */
				if (_listeners.empty()) {
					_pending_contexts.enqueue(c);
					return;
				}
				/* Awake a listener and transmit signal info to it */
				Thread * const t = _listeners.dequeue();
				t->receive_signal(Signal(c->_imprint, c->_number));

				/* Reset context */
				c->_number = 0;
			}
		}

		public:

			/**
			 * Let a thread listen to our contexts
			 */
			void add_listener(Thread * const t)
			{
				t->await_signal();
				_listeners.enqueue(t);
				_listen();
			}

			/**
			 * Recognize that one of our contexts was triggered
			 */
			void add_pending_context(Signal_context * const c)
			{
				assert(c->_receiver == this);
				_pending_contexts.enqueue(c);
				_listen();
			}
	};

	/**
	 * Access to static CPU scheduler
	 */
	Cpu_scheduler * cpu_scheduler()
	{
		/* Create idle thread */
		static char idle_stack[DEFAULT_STACK_SIZE]
		__attribute__((aligned(Cpu::DATA_ACCESS_ALIGNM)));
		static Thread idle((Platform_thread *)0);
		static bool initial = 1;
		if(initial)
		{
			/* Initialize idle thread */
			void * sp;
			sp = (void *)&idle_stack[sizeof(idle_stack)/sizeof(idle_stack[0])];
			idle.init_context((void *)&idle_main, sp, core_id());
			initial = 0;
		}
		/* Create scheduler with a permanent idle thread */
		static unsigned const user_time_slice =
		timer()->ms_to_tics(USER_TIME_SLICE_MS);
		static Cpu_scheduler cpu_sched(&idle, user_time_slice);
		return &cpu_sched;
	}

	/**
	 * Get attributes of the mode transition region in every PD
	 */
	addr_t mode_transition_virt_base() { return mtc()->VIRT_BASE; }
	size_t mode_transition_size() { return mtc()->SIZE; }

	/**
	 * Get attributes of the kernel objects
	 */
	size_t thread_size() { return sizeof(Thread); }
	size_t pd_size() { return sizeof(Pd); }
	size_t signal_context_size() { return sizeof(Signal_context); }
	size_t signal_receiver_size() { return sizeof(Signal_receiver); }
	unsigned pd_alignm_log2() { return Pd::ALIGNM_LOG2; }


	/**
	 * Handle the occurence of an unknown exception
	 */
	void handle_invalid_excpt(Thread * const) { assert(0); }


	/**
	 * Handle an interrupt request
	 */
	void handle_interrupt(Thread * const)
	{
		/* Determine handling for specific interrupt */
		unsigned irq;
		if (pic()->take_request(irq))
		{
			switch(irq)
			{
			case Timer::IRQ: {

				/* Clear interrupt at timer */
				timer()->clear_interrupt();
				break; }

			/* Irq is not owned by core */
			default: {

				/* Notify IRQ owner */
				Irq_owner * const o = Irq_owner::owner(irq);
				assert(o);
				o->receive_irq(irq);
				break; }
			}
		}
		/* Disengage interrupt controller from IRQ */
		pic()->finish_request();
	}


	/**
	 * Handle an usermode pagefault
	 *
	 * \param  user  Thread that has caused the pagefault
	 */
	void handle_pagefault(Thread * const user)
	{
		/* Check out cause and attributes of abort */
		addr_t va;
		bool w;
		assert(user->translation_miss(va, w));

		/* The user might be able to resolve the pagefault */
		user->pagefault(va, w);
	}


	/**
	 * Handle request of an unknown signal type
	 */
	void handle_invalid_syscall(Thread * const) { assert(0); }


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_pd(Thread * const user)
	{
		/* Check permissions */
		assert(user->pd_id() == core_id());

		/* Create PD */
		void * dst = (void *)user->user_arg_1();
		Pd * const pd = new (dst) Pd();

		/* Return success */
		user->user_arg_0(pd->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_thread(Thread * const user)
	{
		/* Check permissions */
		assert(user->pd_id() == core_id());

		/* Dispatch arguments */
		Syscall_arg const arg1 = user->user_arg_1();
		Syscall_arg const arg2 = user->user_arg_2();

		/* Create thread */
		Thread * const t = new ((void *)arg1)
		Thread((Platform_thread *)arg2);

		/* Return thread ID */
		user->user_arg_0((Syscall_ret)t->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_start_thread(Thread * const user)
	{
		/* Check permissions */
		assert(user->pd_id() == core_id());

		/* Dispatch arguments */
		Platform_thread * pt = (Platform_thread *)user->user_arg_1();
		void * const ip = (void *)user->user_arg_2();
		void * const sp = (void *)user->user_arg_3();
		unsigned const cpu = (unsigned)user->user_arg_4();

		/* Get targeted thread */
		Thread * const t = Thread::pool()->object(pt->id());
		assert(t);

		/* Start thread */
		assert(!t->start(ip, sp, cpu, pt->pd_id(),
		                 pt->phys_utcb(), pt->virt_utcb()))

		/* Return software TLB that the thread is assigned to */
		Pd::Pool * const pp = Pd::pool();
		Pd * const pd = pp->object(t->pd_id());
		assert(pd);
		Software_tlb * const tlb = static_cast<Software_tlb *>(pd);
		user->user_arg_0((Syscall_ret)tlb);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_pause_thread(Thread * const user)
	{
		unsigned const tid = user->user_arg_1();

		/* Shortcut for a thread to pause itself */
		if (!tid) {
			user->pause();
			user->user_arg_0(0);
			return;
		}

		/* Get targeted thread and check permissions */
		Thread * const t = Thread::pool()->object(tid);
		assert(t && (user->pd_id() == core_id() || user==t));

		/* Pause targeted thread */
		t->pause();
		user->user_arg_0(0);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_resume_thread(Thread * const user)
	{
		/* Get targeted thread */
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		assert(t);

		/* Check permissions */
		assert(user->pd_id() == core_id() || user->pd_id() == t->pd_id());

		/* Resume targeted thread */
		user->user_arg_0(t->resume());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_yield_thread(Thread * const user)
	{
		/* Get targeted thread */
		Thread * const t = Thread::pool()->object(user->user_arg_1());

		/* Invoke kernel object */
		if (t) t->resume();
		cpu_scheduler()->yield();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_current_thread_id(Thread * const user)
	{
		user->user_arg_0((Syscall_ret)user->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_get_thread(Thread * const user)
	{
		/* Get target */
		unsigned const tid = (unsigned)user->user_arg_1();
		Thread * t;

		/* User targets a thread by ID */
		if (tid) {
			t = Thread::pool()->object(tid);
			assert(t);

		/* User targets itself */
		} else t = user;

		/* Return target platform thread */
		user->user_arg_0((Syscall_ret)t->platform_thread());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_wait_for_request(Thread * const user)
	{
		user->wait_for_request();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_request_and_wait(Thread * const user)
	{
		/* Get IPC receiver */
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		assert(t);

		/* Do IPC */
		user->request_and_wait(t, (size_t)user->user_arg_2());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_reply_and_wait(Thread * const user)
	{
			user->reply_and_wait((size_t)user->user_arg_1());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_set_pager(Thread * const user)
	{
		/* Assertions */
		assert(user->pd_id() == core_id());

		/* Get faulter and pager thread */
		Thread * const p = Thread::pool()->object(user->user_arg_1());
		Thread * const f = Thread::pool()->object(user->user_arg_2());
		assert(p && f);

		/* Assign pager */
		f->pager(p);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_update_pd(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		Cpu::flush_tlb_by_pid(user->user_arg_1());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_allocate_irq(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		unsigned irq = user->user_arg_1();
		user->user_arg_0(user->allocate_irq(irq));
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_free_irq(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		unsigned irq = user->user_arg_1();
		user->user_arg_0(user->free_irq(irq));
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_await_irq(Thread * const user)
	{
			assert(user->pd_id() == core_id());
			user->await_irq();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_print_char(Thread * const user)
	{
		Genode::printf("%c", (char)user->user_arg_1());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_read_register(Thread * const user)
	{
		/* Check permissions */
		assert(user->pd_id() == core_id());

		/* Get targeted thread */
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		assert(t);

		/* Return requested register */
		unsigned gpr;
		assert(t->get_gpr(user->user_arg_2(), gpr));
		user->user_arg_0(gpr);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_write_register(Thread * const user)
	{
		/* Check permissions */
		assert(user->pd_id() == core_id());

		/* Get targeted thread */
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		assert(t);

		/* Write to requested register */
		unsigned const gpr = user->user_arg_3();
		assert(t->set_gpr(user->user_arg_2(), gpr));
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_signal_receiver(Thread * const user)
	{
			/* Check permissions */
			assert(user->pd_id() == core_id());

			/* Create receiver */
			void * dst = (void *)user->user_arg_1();
			Signal_receiver * const r = new (dst) Signal_receiver();

			/* Return success */
			user->user_arg_0(r->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_signal_context(Thread * const user)
	{
		/* Check permissions */
		assert(user->pd_id() == core_id());

		/* Lookup receiver */
		unsigned rid = user->user_arg_2();
		Signal_receiver * const r = Signal_receiver::pool()->object(rid);
		assert(r);

		/* Create context */
		void * dst = (void *)user->user_arg_1();
		unsigned imprint = user->user_arg_3();
		Signal_context * const c = new (dst) Signal_context(r, imprint);

		/* Return success */
		user->user_arg_0(c->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_await_signal(Thread * const user)
	{
		/* Lookup receiver */
		unsigned rid = user->user_arg_2();
		Signal_receiver * const r = Signal_receiver::pool()->object(rid);
		assert(r);

		/* Let user listen to receiver */
		r->add_listener(user);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_submit_signal(Thread * const user)
	{
		/* Lookup context */
		Signal_context * const c =
			Signal_context::pool()->object(user->user_arg_1());
		assert(c);

		/* Trigger signal at context */
		c->trigger_signal(user->user_arg_2());
	}


	void do_switch_vm(Thread * const user)
	{
		Genode::Vm_state *state =
			reinterpret_cast<Genode::Vm_state*>(user->user_arg_1());
		tz_switch_to_normal_world(state);
	}


	/**
	 * Handle a syscall request
	 *
	 * \param  user  Thread that called the syscall
	 */
	void handle_syscall(Thread * const user)
	{
		/* Map syscall types to the according handler functions */
		typedef void (*Syscall_handler)(Thread * const);
		static Syscall_handler const handle_sysc[] = {
			/* 00 */ handle_invalid_syscall,
			/* 01 */ do_new_thread,
			/* 02 */ do_start_thread,
			/* 03 */ do_pause_thread,
			/* 04 */ do_resume_thread,
			/* 05 */ do_get_thread,
			/* 06 */ do_current_thread_id,
			/* 07 */ do_yield_thread,
			/* 08 */ do_request_and_wait,
			/* 09 */ do_reply_and_wait,
			/* 10 */ do_wait_for_request,
			/* 11 */ do_set_pager,
			/* 12 */ do_update_pd,
			/* 13 */ do_new_pd,
			/* 14 */ do_allocate_irq,
			/* 15 */ do_await_irq,
			/* 16 */ do_free_irq,
			/* 17 */ do_print_char,
			/* 18 */ do_read_register,
			/* 19 */ do_write_register,
			/* 20 */ do_new_signal_receiver,
			/* 21 */ do_new_signal_context,
			/* 22 */ do_await_signal,
			/* 23 */ do_submit_signal,
			/* 24 */ do_switch_vm,
		};
		enum { MAX_SYSCALL = sizeof(handle_sysc)/sizeof(handle_sysc[0]) - 1 };

		/* Handle syscall that has been requested by the user */
		unsigned syscall = user->user_arg_0();
		if (syscall > MAX_SYSCALL) handle_sysc[INVALID_SYSCALL](user);
		else handle_sysc[syscall](user);
	}


	/**
	 * Access to board singleton
	 */
	static Genode::Board * board() {
		static Genode::Board _board;
		return &_board;
	}
}


/**
 * Kernel main routine
 */
extern "C" void kernel()
{
	unsigned const timer_value = timer()->stop();
	static unsigned user_time = 0;
	static bool initial_call = true;

	/* An exception occured */
	if (!initial_call)
	{
		/* Update how much time the last user has consumed */
		user_time = timer_value < user_time ? user_time - timer_value : 0;

		/* Map exception types to exception-handler functions */
		typedef void (*Exception_handler)(Thread * const);
		static Exception_handler const handle_excpt[] = {
			/* 0 */ handle_invalid_excpt,
			/* 1 */ handle_interrupt,
			/* 2 */ handle_pagefault,
			/* 3 */ handle_syscall
		};
		/* Handle exception that interrupted the last user */
		Thread * const user = cpu_scheduler()->current_entry();
		enum { MAX_EXCPT = sizeof(handle_excpt)/sizeof(handle_excpt[0]) - 1 };
		unsigned const e = user->exception();
		if (e > MAX_EXCPT) handle_invalid_excpt(user);
		else handle_excpt[e](user);

	/* Kernel initialization */
	} else {

		/* Tell the code that has called kernel,
		 * what to do when kernel returns */
		_call_after_kernel = mtc()->virt_user_entry();

		/* Compose core address space */
		addr_t a = 0;
		while (1)
		{
			/* Map everything except the mode transition region */
			enum {
				SIZE_LOG2 = Software_tlb::MAX_TRANSL_SIZE_LOG2,
				SIZE = 1 << SIZE_LOG2,
			};
			if (mtc()->VIRT_END <= a || mtc()->VIRT_BASE > (a + SIZE - 1))
			{
				/* Map 1:1 with rwx permissions */
				if (core()->insert_translation(a, a, SIZE_LOG2, 1, 1, 0, 0))
					assert(0);
			}
			/* Check condition to continue */
			addr_t const next_a = a + SIZE;
			if (next_a > a) a = next_a;
			else break;
		}

		/* Compose kernel CPU context */
		static Cpu::Context kernel_context;
		kernel_context.instruction_ptr((addr_t)kernel);
		kernel_context.return_ptr(mtc()->virt_user_entry());
		kernel_context.stack_ptr((addr_t)&_kernel_stack_high);

		/* Add kernel to the core PD */
		core()->append_context(&kernel_context);

		/* Offer the final kernel context to the mode transition page */
		mtc()->fetch_kernel_context(&kernel_context);

		/* Enable Trustzone support if available */
		board()->enable_trustzone();

		/* Switch to core address space */
		Cpu::enable_mmu(core(), core_id());

		/* Create the core main thread */
		static Native_utcb cm_utcb;
		static char cm_stack[DEFAULT_STACK_SIZE]
		            __attribute__((aligned(Cpu::DATA_ACCESS_ALIGNM)));
		static Thread core_main((Platform_thread *)0);
		_main_utcb = &cm_utcb;
		enum { CM_STACK_SIZE = sizeof(cm_stack)/sizeof(cm_stack[0]) + 1 };
		core_main.start((void *)&CORE_MAIN,
		                (void *)&cm_stack[CM_STACK_SIZE - 1],
		                0, core_id(), &cm_utcb, &cm_utcb);

		/* Kernel initialization finished */
		initial_call = false;
	}
	/* Offer next user context to the mode transition PIC */
	Thread * const next = cpu_scheduler()->next_entry(user_time);
	mtc()->user_context(next);

	/* Limit user mode execution in time */
	timer()->start_one_shot(user_time);
	pic()->unmask(Timer::IRQ);
}


/********************
 ** Kernel::Thread **
 ********************/

int Thread::start(void *ip, void *sp, unsigned cpu_no,
                         unsigned const pd_id,
                         Native_utcb * const phys_utcb,
                         Native_utcb * const virt_utcb)
{
	/* Check state and arguments */
	assert(_state == STOPPED)
	assert(!cpu_no);

	/* Apply thread configuration */
	init_context(ip, sp, pd_id);
	_phys_utcb = phys_utcb;
	_virt_utcb = virt_utcb;

	/* Offer thread-entry arguments */
	user_arg_0((unsigned)_virt_utcb);

	/* Start thread */
	cpu_scheduler()->insert(this);
	_state = ACTIVE;
	return 0;
}


void Thread::init_context(void * const ip, void * const sp,
                                  unsigned const pd_id)
{
	/* Basic thread state */
	stack_ptr((addr_t)sp);
	instruction_ptr((addr_t)ip);

	/* Join a pd */
	_pd_id = pd_id;
	Pd * const pd = Pd::pool()->object(_pd_id);
	assert(pd)
	protection_domain(pd_id);
	software_tlb(pd);
}


void Thread::pagefault(addr_t const va, bool const w)
{
	/* Pause faulter */
	cpu_scheduler()->remove(this);
	_state = AWAIT_RESUMPTION;

	/* Inform pager through IPC */
	assert(_pager);
	Software_tlb * const tlb =
	static_cast<Software_tlb *>(software_tlb());
	_pagefault =
	Pagefault(id(), tlb, instruction_ptr(), va, w);
	Ipc_node::send_note(_pager, &_pagefault, sizeof(_pagefault));
}


/****************************
 ** Kernel::Signal_context **
 ****************************/


void Signal_context::trigger_signal(unsigned const number)
{
	/* Raise our number */
	unsigned const old_nr = _number;
	_number += number;
	assert(old_nr <= _number);

	/* Notify our receiver */
	if (_number) _receiver->add_pending_context(this);
}

