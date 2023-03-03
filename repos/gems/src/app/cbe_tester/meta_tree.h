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
#include <module.h>

namespace Cbe
{
	class Meta_tree;
	class Meta_tree_request;
	class Meta_tree_channel;
}

class Cbe::Meta_tree_request : public Module_request
{
	public:

		enum Type { INVALID = 0, COW_UPDATE = 1 };

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
		Genode::uint64_t _curr_gen            { 0 };
		Genode::uint64_t _old_pba             { 0 };
		Genode::uint64_t _new_pba             { 0 };
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
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE,
			ENCRYPT_CLIENT_DATA_PENDING,
			ENCRYPT_CLIENT_DATA_IN_PROGRESS,
			ENCRYPT_CLIENT_DATA_COMPLETE,
			DECRYPT_CLIENT_DATA_PENDING,
			DECRYPT_CLIENT_DATA_IN_PROGRESS,
			DECRYPT_CLIENT_DATA_COMPLETE
		};

		State             _state                    { INACTIVE };
		Meta_tree_request _request                  { };
		Vfs::file_offset  _nr_of_processed_bytes    { 0 };
		Vfs::file_size    _nr_of_remaining_bytes    { 0 };
		char              _blk_buf[Cbe::BLOCK_SIZE] { 0 };
		bool              _generated_req_success    { false };
};

class Cbe::Meta_tree : public Module
{
	private:

		using Request = Meta_tree_request;
		using Channel = Meta_tree_channel;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using file_size = Vfs::file_size;
		using file_offset = Vfs::file_offset;

		enum { NR_OF_CHANNELS = 1 };

		String<32> const  _path;
		Vfs::Env         &_vfs_env;
		Vfs::Vfs_handle  &_vfs_handle               { vfs_open_rw(_vfs_env, _path) };
		Channel           _channels[NR_OF_CHANNELS] { };

		void _execute_read(Channel &channel,
		                   bool    &progress);

		void _execute_write(Channel &channel,
		                    bool    &progress);

		void _execute_read_client_data(Channel &channel,
		                               bool    &progress);

		void _execute_write_client_data(Channel &channel,
		                                bool    &progress);

		void _execute_sync(Channel &channel,
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

		Meta_tree(Vfs::Env               &vfs_env,
		          Genode::Xml_node const &xml_node);


		/************
		 ** Module **
		 ************/
};

#endif /* _META_TREE_H_ */
