/*
 * \brief  Module for request pool
 * \author Martin Stein
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REQUEST_POOL_H_
#define _REQUEST_POOL_H_

/* cbe tester includes */
#include <module.h>
#include <cbe/types.h>
#include <vfs_utilities.h>

namespace Cbe
{
	class Request_pool;
	class Request_pool_request;
	class Request_pool_channel;


	class Request : public Module_request
	{
		public:

			enum Operation : uint32_t {
				INVALID = 0,
				READ = 1,
				WRITE = 2,
				SYNC = 3,
				CREATE_SNAPSHOT = 4,
				DISCARD_SNAPSHOT = 5,
				REKEY = 6,
				EXTEND_VBD = 7,
				EXTEND_FT = 8,
				RESUME_REKEYING = 10,
				DEINITIALIZE = 11,
				INITIALIZE = 12,
			};

		private:

			Operation            _operation;
			bool                 _success;
			uint64_t             _block_number;
			uint64_t             _offset;
			Number_of_blocks_old _count;
			uint32_t             _key_id;
			uint32_t             _tag;

		public:

			Request(Operation            operation,
			        bool                 success,
			        uint64_t             block_number,
			        uint64_t             offset,
			        Number_of_blocks_old count,
			        uint32_t             key_id,
			        uint32_t             tag,
			        unsigned long        src_module_id,
			        unsigned long        src_request_id)
			:
				Module_request { src_module_id, src_request_id, REQUEST_POOL },
				_operation     { operation    },
				_success       { success      },
				_block_number  { block_number },
				_offset        { offset       },
				_count         { count        },
				_key_id        { key_id       },
				_tag           { tag          }
			{ }

			Request()
			:
				Module_request { },
				_operation     { Operation::INVALID },
				_success       { false },
				_block_number  { 0 },
				_offset        { 0 },
				_count         { 0 },
				_key_id        { 0 },
				_tag           { 0 }
			{ }

			bool valid() const
			{
				return _operation != Operation::INVALID;
			}

			void print(Genode::Output &out) const;


			/***************
			 ** Accessors **
			 ***************/

			bool read()             const { return _operation == Operation::READ; }
			bool write()            const { return _operation == Operation::WRITE; }
			bool sync()             const { return _operation == Operation::SYNC; }
			bool create_snapshot()  const { return _operation == Operation::CREATE_SNAPSHOT; }
			bool discard_snapshot() const { return _operation == Operation::DISCARD_SNAPSHOT; }
			bool rekey()            const { return _operation == Operation::REKEY; }
			bool extend_vbd()       const { return _operation == Operation::EXTEND_VBD; }
			bool extend_ft()        const { return _operation == Operation::EXTEND_FT; }
			bool resume_rekeying()  const { return _operation == Operation::RESUME_REKEYING; }
			bool deinitialize()     const { return _operation == Operation::DEINITIALIZE; }
			bool initialize()       const { return _operation == Operation::INITIALIZE; }

			Operation            operation()    const { return _operation; }
			bool                 success()      const { return _success; }
			uint64_t             block_number() const { return _block_number; }
			uint64_t             offset()       const { return _offset; }
			Number_of_blocks_old count()        const { return _count; }
			uint32_t             key_id()       const { return _key_id; }
			uint32_t             tag()          const { return _tag; }

			void offset(uint64_t arg) { _offset = arg; }
			void success(bool arg) { _success = arg; }
			void tag(uint32_t arg)    { _tag = arg; }

			char const *type_name() override;

	} __attribute__((packed));

}

class Cbe::Request_pool_channel
{
	private:

		friend class Request_pool;

		enum State {
			INVALID,
			SUBMITTED,
			SUBMITTED_RESUME_REKEYING,
			REKEY_INIT_PENDING,
			REKEY_INIT_IN_PROGRESS,
			REKEY_INIT_COMPLETE,
			PREPONE_REQUESTS_PENDING,
			PREPONE_REQUESTS_COMPLETE,
			VBD_EXTENSION_STEP_PENDING,
			VBD_EXTENSION_STEP_IN_PROGRESS,
			VBD_EXTENSION_STEP_COMPLETE,
			FT_EXTENSION_STEP_PENDING,
			FT_EXTENSION_STEP_IN_PROGRESS,
			FT_EXTENSION_STEP_COMPLETE,
			CREATE_SNAP_AT_SB_CTRL_PENDING,
			CREATE_SNAP_AT_SB_CTRL_IN_PROGRESS,
			CREATE_SNAP_AT_SB_CTRL_COMPLETE,
			SYNC_AT_SB_CTRL_PENDING,
			SYNC_AT_SB_CTRL_IN_PROGRESS,
			SYNC_AT_SB_CTRL_COMPLETE,
			READ_VBA_AT_SB_CTRL_PENDING,
			READ_VBA_AT_SB_CTRL_IN_PROGRESS,
			READ_VBA_AT_SB_CTRL_COMPLETE,
			WRITE_VBA_AT_SB_CTRL_PENDING,
			WRITE_VBA_AT_SB_CTRL_IN_PROGRESS,
			WRITE_VBA_AT_SB_CTRL_COMPLETE,
			DISCARD_SNAP_AT_SB_CTRL_PENDING,
			DISCARD_SNAP_AT_SB_CTRL_IN_PROGRESS,
			DISCARD_SNAP_AT_SB_CTRL_COMPLETE,
			REKEY_VBA_PENDING,
			REKEY_VBA_IN_PROGRESS,
			REKEY_VBA_COMPLETE,
			INITIALIZE_SB_CTRL_PENDING,
			INITIALIZE_SB_CTRL_IN_PROGRESS,
			INITIALIZE_SB_CTRL_COMPLETE,
			DEINITIALIZE_SB_CTRL_PENDING,
			DEINITIALIZE_SB_CTRL_IN_PROGRESS,
			DEINITIALIZE_SB_CTRL_COMPLETE,
			COMPLETE
		};

		enum Tag_type {
			TAG_POOL_SB_CTRL_READ_VBA,
			TAG_POOL_SB_CTRL_WRITE_VBA,
			TAG_POOL_SB_CTRL_SYNC,
			TAG_POOL_SB_CTRL_INITIALIZE,
			TAG_POOL_SB_CTRL_DEINITIALIZE,
		};

		using Index_type = Genode::uint32_t; /* XXX */

		struct Generated_prim {
			enum Type { READ, WRITE };

			Type       op;
			bool       succ;
			Tag_type   tg;
			Index_type pl_idx;
			uint64_t   blk_nr;
			uint64_t   idx;
		};

		Cbe::Request     _request    { };
		State            _state      { INVALID };
		Generated_prim   _prim       { };
		uint64_t         _nr_of_blks { 0 };
		Generation       _gen        { };
		Superblock_state _sb_state   { Superblock_state::INVALID };

		void invalidate()
		{
			_request    = { };
			_state      = { INVALID };
			_prim       = { };
			_nr_of_blks =  0;
			_gen        = { };
			_sb_state   = { Superblock_state::INVALID };
		}
};

class Cbe::Request_pool : public Module
{
	private:

		using Channel = Request_pool_channel;
		using Request = Cbe::Request;
		using Slots_index = Genode::uint32_t;

		enum { NR_OF_CHANNELS = 16 };

		struct Index_queue
		{
			Slots_index _head                  { 0 };
			Slots_index _tail                  { 0 };
			unsigned    _nr_of_used_slots      { 0 };
			Slots_index _slots[NR_OF_CHANNELS] { 0 };

			bool empty() const { return _nr_of_used_slots == 0; }

			bool full() const {
				return _nr_of_used_slots >= NR_OF_CHANNELS; }

			Slots_index head() const
			{
				if (empty()) {
					class Index_queue_empty_head { };
					throw Index_queue_empty_head { };
				}
				return _slots[_head];
			}

			void enqueue(Slots_index const idx)
			{
				if (full()) {
					class Index_queue_enqueue_full { };
					throw Index_queue_enqueue_full { };
				}

				_slots[_tail] = idx;

				_tail = (_tail + 1) % NR_OF_CHANNELS;

				_nr_of_used_slots += 1;
			}

			void dequeue(Slots_index const idx)
			{
				if (empty() or head() != idx) {
					class Index_queue_dequeue_error { };
					throw Index_queue_dequeue_error { };
				}

				_head = (_head + 1) % NR_OF_CHANNELS;

				_nr_of_used_slots -= 1;
			}
		};

		Channel     _channels[NR_OF_CHANNELS] { };
		Index_queue _indices { };

		void _execute_read (Channel &, Index_queue &, Slots_index const, bool &);

		void _execute_write(Channel &, Index_queue &, Slots_index const, bool &);

		void _execute_sync (Channel &, Index_queue &, Slots_index const, bool &);

		void _execute_initialize(Channel &, Index_queue &, Slots_index const,
		                         bool &);
		void _execute_deinitialize(Channel &, Index_queue &, Slots_index const,
		                           bool &);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

	public:

		Request_pool();


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override { return !_indices.full(); }

		void submit_request(Module_request &req) override;
};


inline char const *to_string(Cbe::Request::Operation op)
{
	struct Unknown_operation_type : Genode::Exception { };
	switch (op) {
	case Cbe::Request::Operation::INVALID: return "invalid";
	case Cbe::Request::Operation::READ: return "read";
	case Cbe::Request::Operation::WRITE: return "write";
	case Cbe::Request::Operation::SYNC: return "sync";
	case Cbe::Request::Operation::CREATE_SNAPSHOT: return "create_snapshot";
	case Cbe::Request::Operation::DISCARD_SNAPSHOT: return "discard_snapshot";
	case Cbe::Request::Operation::REKEY: return "rekey";
	case Cbe::Request::Operation::EXTEND_VBD: return "extend_vbd";
	case Cbe::Request::Operation::EXTEND_FT: return "extend_ft";
	case Cbe::Request::Operation::RESUME_REKEYING: return "resume_rekeying";
	case Cbe::Request::Operation::DEINITIALIZE: return "deinitialize";
	case Cbe::Request::Operation::INITIALIZE: return "initialize";
	}
	throw Unknown_operation_type();
}


inline void Cbe::Request::print(Genode::Output &out) const
{
	if (!valid()) {
		Genode::print(out, "<invalid>");
		return;
	}
	Genode::print(out, "op=", to_string (_operation));
	Genode::print(out, " vba=", _block_number);
	Genode::print(out, " cnt=", _count);
	Genode::print(out, " tag=", _tag);
	Genode::print(out, " key=", _key_id);
	Genode::print(out, " off=", _offset);
	Genode::print(out, " succ=", _success);
}

#endif /* _REQUEST_POOL_H_ */
