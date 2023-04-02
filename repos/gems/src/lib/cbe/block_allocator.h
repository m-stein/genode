/*
 * \brief  Module for managing the block allocation
 * \author Josef Soentgen
 * \date   2023-02-28
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BLOCK_ALLOCATOR_H_
#define _BLOCK_ALLOCATOR_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>

namespace Cbe
{
	class Block_allocator;
	class Block_allocator_request;
	class Block_allocator_channel;
}


class Cbe::Block_allocator_request : public Module_request
{
	public:

		enum Type { INVALID = 0, GET = 1, };

	private:

		friend class Block_allocator;
		friend class Block_allocator_channel;

		Type             _type                { INVALID };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE] { 0 };
		Genode::uint64_t _blk_nr              { 0 };
		bool             _success             { false };

	public:

		Block_allocator_request() { }

		Block_allocator_request(unsigned long src_module_id,
		                        unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   size_t            buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   Genode::size_t    prim_size);

		void *prim_ptr() { return (void *)&_prim; }

		Genode::uint64_t blk_nr() const { return _blk_nr; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Genode::Output &out) const override;
};


class Cbe::Block_allocator_channel
{
	private:

		friend class Block_allocator;

		enum State { INACTIVE, SUBMITTED, PENDING, COMPLETE };

		State                   _state   { INACTIVE };
		Block_allocator_request _request { };
};


class Cbe::Block_allocator : public Module
{
	private:

		using Request = Block_allocator_request;
		using Channel = Block_allocator_channel;

		uint64_t const _first_block;
		uint64_t       _nr_of_blks;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _execute_get(Channel &channel,
		                  bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

	public:

		Block_allocator(uint64_t first_block);

		uint64_t first_block() const { return _first_block; }

		uint64_t nr_of_blks() const { return _nr_of_blks; }

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _BLOCK_ALLOCATOR_H_ */
