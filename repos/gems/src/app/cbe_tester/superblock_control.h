//
//		/**
//		 * Query list of active snapshots
//		 *
//		 * \param  ids  reference to destination buffer
//		 */
//		void active_snapshot_ids(Active_snapshot_ids &ids) const;
//
//		/**
//		 * Get highest virtual-block-address useable by the current active snapshot
//		 *
//		 * \return  highest addressable virtual-block-address
//		 */
//		Virtual_block_address max_vba() const;
//
//		/**
//		 * Get information about the CBE
//		 *
//		 * \return  information structure
//		 */
//		Info info() const
//		{
//			Info inf { };
//			_info(inf);
//			return inf;
//		}

/*
 * \brief  Module for management of the superblocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SUPERBLOCK_CONTROL_H_
#define _SUPERBLOCK_CONTROL_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>

namespace Cbe
{
	class Superblock_control;
	class Superblock_control_request;
	class Superblock_control_channel;
}

class Cbe::Superblock_control_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, READ_VBA = 1, WRITE_VBA = 2, SYNC = 3, INITIALIZE = 4,
			DEINITIALIZE = 5 };

	private:

		friend class Superblock_control;
		friend class Superblock_control_channel;

		Type                  _type                    { INVALID };
		Genode::uint64_t      _client_req_offset       { 0 };
		Genode::uint64_t      _client_req_tag          { 0 };
		Virtual_block_address _vba                     { 0 };
		Genode::uint8_t       _prim[PRIM_BUF_SIZE]     { 0 };
		bool                  _success                 { false };

	public:

		Superblock_control_request() { }

		Type type() const { return _type; }

		Superblock_control_request(unsigned long src_module_id,
		                           unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   size_t            prim_size,
		                   Genode::uint64_t  client_req_offset,
		                   Genode::uint64_t  client_req_tag,
		                   Genode::uint64_t  vba);

		void *prim_ptr() { return (void *)&_prim; }

		Superblock_state sb_state() { class Exception_1 { }; throw Exception_1 { }; }

		bool success() const { return _success; }


		/********************
		 ** Module_request **
		 ********************/

		char const *type_name() override;
};

class Cbe::Superblock_control_channel
{
	private:

		friend class Superblock_control;

		enum State {
			INACTIVE, SUBMITTED, COMPLETE };

		State                      _state   { INACTIVE };
		Superblock_control_request _request { };

	public:

		Superblock_control_request const &request() const { return _request; }
};

class Cbe::Superblock_control : public Module
{
	private:

		using Request = Superblock_control_request;
		using Channel = Superblock_control_channel;

		enum { NR_OF_CHANNELS = 1 };

		Channel _channels[NR_OF_CHANNELS] { };


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
};

#endif /* _SUPERBLOCK_CONTROL_H_ */
