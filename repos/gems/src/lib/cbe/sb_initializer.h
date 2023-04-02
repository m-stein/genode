/*
 * \brief  Module for initializing the SB
 * \author Josef Soentgen
 * \date   2023-03-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SB_INITIALIZER_H_
#define _SB_INITIALIZER_H_

/* Genode includes */
#include <base/output.h>

/* cbe tester includes */
#include "module.h"

namespace Cbe
{
	class Sb_initializer;
	class Sb_initializer_request;
	class Sb_initializer_channel;
}


class Cbe::Sb_initializer_request : public Module_request
{
	public:

		enum Type { INVALID = 0, INIT = 1, };

	private:

		friend class Sb_initializer;
		friend class Sb_initializer_channel;

		Type             _type                { INVALID };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE] { 0 };

		Tree_level_index      _vbd_max_level_idx { 0 };
		Tree_degree           _vbd_max_child_idx { 0 };
		Tree_number_of_leaves _vbd_nr_of_leaves  { 0 };
		Tree_level_index      _ft_max_level_idx  { 0 };
		Tree_degree           _ft_max_child_idx  { 0 };
		Tree_number_of_leaves _ft_nr_of_leaves   { 0 };
		Tree_level_index      _mt_max_level_idx  { 0 };
		Tree_degree           _mt_max_child_idx  { 0 };
		Tree_number_of_leaves _mt_nr_of_leaves   { 0 };
		bool _success { false };


	public:

		Sb_initializer_request() { }

		Sb_initializer_request(unsigned long src_module_id,
		                        unsigned long src_request_id);

		static void create(void                  *buf_ptr,
		                   size_t                 buf_size,
		                   Genode::uint64_t       src_module_id,
		                   Genode::uint64_t       src_request_id,
		                   Genode::size_t         req_type,
		                   void                  *prim_ptr,
		                   Genode::size_t         prim_size,
		                   Tree_level_index       vbd_max_level_idx,
		                   Tree_degree            vbd_max_child_idx,
		                   Tree_number_of_leaves  vbd_nr_of_leaves,
		                   Tree_level_index       ft_max_level_idx,
		                   Tree_degree            ft_max_child_idx,
		                   Tree_number_of_leaves  ft_nr_of_leaves,
		                   Tree_level_index       mt_max_level_idx,
		                   Tree_degree            mt_max_child_idx,
		                   Tree_number_of_leaves  mt_nr_of_leaves);

		void *prim_ptr() { return (void *)&_prim; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Genode::Output &out) const override { Genode::print(out, type_to_string(_type)); }
};


class Cbe::Sb_initializer_channel
{
	private:

		friend class Sb_initializer;

		enum State {
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, SLOT_COMPLETE, COMPLETE,
			FT_REQUEST_COMPLETE,
			FT_REQUEST_IN_PROGRESS,
			FT_REQUEST_PENDING,
			MT_REQUEST_COMPLETE,
			MT_REQUEST_IN_PROGRESS,
			MT_REQUEST_PENDING,
			SYNC_REQUEST_COMPLETE,
			SYNC_REQUEST_IN_PROGRESS,
			SYNC_REQUEST_PENDING,
			TA_REQUEST_CREATE_KEY_COMPLETE,
			TA_REQUEST_CREATE_KEY_IN_PROGRESS,
			TA_REQUEST_CREATE_KEY_PENDING,
			TA_REQUEST_ENCRYPT_KEY_COMPLETE,
			TA_REQUEST_ENCRYPT_KEY_IN_PROGRESS,
			TA_REQUEST_ENCRYPT_KEY_PENDING,
			TA_REQUEST_SECURE_SB_COMPLETE,
			TA_REQUEST_SECURE_SB_IN_PROGRESS,
			TA_REQUEST_SECURE_SB_PENDING,
			VBD_REQUEST_COMPLETE,
			VBD_REQUEST_IN_PROGRESS,
			VBD_REQUEST_PENDING,
			WRITE_REQUEST_COMPLETE,
			WRITE_REQUEST_IN_PROGRESS,
			WRITE_REQUEST_PENDING,
		};

		State                  _state   { INACTIVE };
		Sb_initializer_request _request { };

		Superblocks_index _sb_slot_index { 0 };

		Superblock  _sb      { };
		Block_data  _sb_slot { };

		Block_data _blk_io_data { };
		Key_new    _key_plain   { };
		Key_new    _key_cipher  { };
		Hash_new   _sb_hash     { };

		Type_1_node _vbd_node { };
		Type_1_node _ft_node  { };
		Type_1_node _mt_node  { };

		bool _generated_req_success { false };

		void clean_data()
		{
			_sb.initialize_invalid();
			memset(&_sb_slot, 0, sizeof(_sb_slot));

			memset(&_blk_io_data, 0, sizeof(_blk_io_data));
			memset(&_key_plain,   0, sizeof(_key_plain));
			memset(&_key_cipher,  0, sizeof(_key_cipher));
			memset(&_sb_hash,     0, sizeof(_sb_hash));

			memset(&_vbd_node, 0, sizeof(_vbd_node));
			memset(&_ft_node,  0, sizeof(_ft_node));
			memset(&_mt_node,  0, sizeof(_mt_node));
		}
};


class Cbe::Sb_initializer : public Module
{
	private:

		using Request = Sb_initializer_request;
		using Channel = Sb_initializer_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };

		void _populate_sb_slot(Channel                &channel,
		                       Physical_block_address  first,
		                       Number_of_blocks        num);

		void _execute(Channel &channel,
		              bool    &progress);

		void _execute_init(Channel &channel,
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

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;


	public:

		Sb_initializer();

		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;

};

#endif /* _SB_INITIALIZER_H_ */
