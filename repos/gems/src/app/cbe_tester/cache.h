/*
 * \brief  Module for cached access to physical blocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CACHE_H_
#define _CACHE_H_

/* cbe tester includes */
#include <module.h>

namespace Cbe
{
	class Cache;
	class Cache_request;
	class Cache_channel;
}

class Cbe::Cache_request : public Module_request
{
	public:

		enum Type { INVALID = 0, READ = 1, WRITE = 2, SYNC = 3 };

	private:

		friend class Cache;
		friend class Cache_channel;

		Type             _type                { INVALID };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE] { 0 };
		Genode::uint64_t _pba                 { 0 };
		Genode::addr_t   _blk_ptr             { 0 };
		bool             _success             { false };

	public:

		Cache_request() { }

		Cache_request(unsigned long src_module_id,
		              unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   Genode::size_t    prim_size,
		                   Genode::uint64_t  pba,
		                   void             *blk_ptr);

		void *prim_ptr() { return (void *)&_prim; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		char const *type_name() override { return type_to_string(_type); }
};

#endif /* _CACHE_H_ */
