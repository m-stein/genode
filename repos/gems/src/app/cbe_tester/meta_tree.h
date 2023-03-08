/*
 * \brief  Module for operating on the meta tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _META_TREE_H_
#define _META_TREE_H_

/* cbe tester includes */
#include <cbe_types.h>

/* cbe tester includes */
#include <module.h>
#include <sha256_4k_hash.h>

namespace Cbe
{
	class Meta_tree;
	class Meta_tree_request;
	class Meta_tree_channel;
}

class Cbe::Meta_tree_request : public Module_request
{
	public:

		enum Type { INVALID = 0, UPDATE = 1 };

	private:

		friend class Meta_tree;
		friend class Meta_tree_channel;

		Type             _type                { INVALID };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE] { 0 };
		Genode::addr_t   _mt_root_pba_ptr     { 0 };
		Genode::addr_t   _mt_root_gen_ptr     { 0 };
		Genode::addr_t   _mt_root_hash_ptr    { 0 };
		Genode::uint64_t _mt_max_lvl          { 0 };
		Genode::uint64_t _mt_edges            { 0 };
		Genode::uint64_t _mt_leaves           { 0 };
		Genode::uint64_t _current_gen         { 0 };
		Genode::uint64_t _old_pba             { INVALID_PBA };
		Genode::uint64_t _new_pba             { INVALID_PBA };
		bool             _success             { false };

	public:

		Meta_tree_request() { }

		Meta_tree_request(unsigned long src_module_id,
		                  unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   Genode::size_t    prim_size,
		                   void             *mt_root_pba_ptr,
		                   void             *mt_root_gen_ptr,
		                   void             *mt_root_hash_ptr,
		                   Genode::uint64_t  mt_max_lvl,
		                   Genode::uint64_t  mt_edges,
		                   Genode::uint64_t  mt_leaves,
		                   Genode::uint64_t  curr_gen,
		                   Genode::uint64_t  old_pba);

		void *prim_ptr() { return (void *)&_prim; }

		Genode::uint64_t new_pba() { return _new_pba; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		char const *type_name() override { return type_to_string(_type); }
};


class Cbe::Meta_tree_channel
{
	private:

		friend class Meta_tree;

		enum State {
			INVALID,
			UPDATE,
			COMPLETE,
			TREE_HASH_MISMATCH
		};

		struct Local_cache_request
		{
			enum State { INVALID, PENDING, IN_PROGRESS };
			enum Op { READ, WRITE, SYNC };

			State            state                  { INVALID };
			Op               op                     { READ };
			bool             success                { false };
			Genode::uint64_t pba                    { 0 };
			Genode::uint64_t level                  { 0 };
			Genode::uint8_t  block_data[BLOCK_SIZE] { 0 };

			Local_cache_request(State             state,
			                    Op                op,
			                    bool              success,
			                    Genode::uint64_t  pba,
			                    Genode::uint64_t  level,
			                    Genode::uint8_t  *blk_ptr)
			:
				state   { state },
				op      { op },
				success { success },
				pba     { pba },
				level   { level }
			{
				if (blk_ptr != nullptr) {
					Genode::memcpy(&block_data, blk_ptr, BLOCK_SIZE);
				}
			}

			Local_cache_request() { }
		};

		State               _state                                { INVALID };
		Meta_tree_request   _request                              { };
		Local_cache_request _cache_request                        { };
		Genode::uint8_t     _blk_io_data[BLOCK_SIZE]              { 0 };
		Type_2_info         _level_1_node                         { };
		Type_1_info         _level_n_nodes[TREE_MAX_NR_OF_LEVELS] { };
		bool                _finished                             { false };
		bool                _root_dirty                           { false };
};

class Cbe::Meta_tree : public Module
{
	private:

		using Request = Meta_tree_request;
		using Channel = Meta_tree_channel;
		using Local_cache_request = Channel::Local_cache_request;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _handle_level_n_nodes(Channel &channel,
		                           bool    &handled);

		void _handle_level_1_node(Channel &channel,
		                          bool    &handled);

		void _exchange_request_pba(Channel     &channel,
		                           Type_2_node &t2_entry);

		void _exchange_nv_inner_nodes(Channel     &channel,
		                              Type_2_node &t2_entry,
		                              bool        &exchanged);

		void _exchange_nv_level_1_node(Channel     &channel,
		                               Type_2_node &t2_entry,
		                               bool        &exchanged);

		bool _node_volatile(Type_1_node      t1_node,
		                    Genode::uint64_t gen);

		void _handle_level_0_nodes(Channel &channel,
		                           bool    &handled);

		void _update_parent(Type_1_node           &node,
		                    Genode::uint8_t const *blk_ptr,
		                    Genode::uint64_t       gen,
		                    Genode::uint64_t       pba);

		void _handle_level_0_nodes(bool &handled);

		void _execute_update(Channel &channel,
		                     bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

	public:

		Meta_tree();
};

#endif /* _META_TREE_H_ */
