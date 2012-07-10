/*
 * \brief  Indexing over all objects of one type
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__INDEXED_H_
#define _INCLUDE__UTIL__INDEXED_H_

/* Genode includes */
#include <base/object_pool.h>

/* local includes */
#include <util/assert.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Map unique sortable IDs to object pointers
	 *
	 * \param  OBJECT_T  object type that should be inherited
	 *                   from 'Object_pool::Entry'
	 */
	template <typename _OBJECT_T>
	class Object_pool
	{
		public:

			typedef _OBJECT_T Object;

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

			~Object_pool() { }

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
	 * Wrapper for 'Object_pool' that synchronizes access
	 */
	template <typename _OBJECT_T>
	class Synchronized_object_pool : public Object_pool<_OBJECT_T>
	{
		typedef Object_pool<_OBJECT_T> Async;
		Lock _lock;

		public:

			~Synchronized_object_pool() { }

			/*****************
			 ** Object_pool **
			 *****************/

			void insert(_OBJECT_T * const object)
			{
				Lock::Guard guard(_lock);
				Async::insert(object);
			}

			void remove(_OBJECT_T * const object)
			{
				Lock::Guard guard(_lock);
				Async::remove(object);
			}

			_OBJECT_T * object(unsigned const id)
			{
				Lock::Guard guard(_lock);
				return Async::object(id);
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
			 * Destructor
			 */
			~Id_allocator() { }

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
	 * Index 'T'-objects via unique IDs if 'T' derives from this class
	 */
	template <typename T, unsigned MAX_OBJECTS>
	class Indexed : public Synchronized_object_pool<T>::Entry
	{
		T * const _derivate; /* proofed pointer to deriving object */

		typedef Synchronized_object_pool<T> Pool;
		typedef Id_allocator<MAX_OBJECTS> Ids;

		/**
		 * Allocator for unique IDs for 'T'-objects
		 */
		static Ids * _ids()
		{
			static Ids _o;
			return &_o;
		}

		/**
		 * Pool that maps 'T'-object IDs to 'T'-objects
		 */
		static Pool * _pool()
		{
			static Pool _o;
			return &_o;
		}

		public:

			/**
			 * Get a 'T'-object by its ID or 0 if it doesn't exist
			 */
			static T * by_id(unsigned const id)
			{ return _pool()->object(id); }

			/**
			 * Constructor
			 */
			Indexed() :
				Pool::Entry(_ids()->alloc()),
				_derivate(static_cast<T *>(this))
			{ _pool()->insert(_derivate); }

			/**
			 * Destructor
			 */
			~Indexed()
			{
				_pool()->remove(_derivate);
				_ids()->free(Pool::Entry::id());
			}
	};

	/**
	 * Index 'T'-objects via capabilities if 'T' derives from this class
	 */
	template <typename T>
	class Capability_indexed : public Genode::Object_pool<T>::Entry
	{
		typedef Genode::Object_pool<T> Pool;

		/**
		 * Pool that maps capabilities to 'T'-objects
		 */
		static Pool * _pool()
		{
			static Pool _o;
			return &_o;
		}

		T * const _derivate; /* proofed pointer to deriving object */

		public:

			/**
			 * Get a 'T'-object by its cap or 0 if it doesn't exist
			 */
			static T * by_cap(Untyped_capability cap)
			{ return _pool()->obj_by_cap(cap); }

			/**
			 * Constructor
			 *
			 * \param  cap  identifying capability
			 */
			Capability_indexed(Untyped_capability cap) :
				Pool::Entry(cap),
				_derivate(static_cast<T *>(this))
			{ _pool()->insert(_derivate); }

			/**
			 * Destructor
			 */
			~Capability_indexed() { _pool()->remove(_derivate); }
	};
}

#endif /* _INCLUDE__UTIL__INDEXED_H_ */

