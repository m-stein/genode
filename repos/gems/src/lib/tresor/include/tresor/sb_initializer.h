/*
 * \brief  Module for initializing the superblocks of a new Tresor
 * \author Josef Soentgen
 * \date   2023-03-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__SB_INITIALIZER_H_
#define _TRESOR__SB_INITIALIZER_H_

/* base includes */
#include <base/output.h>

/* tresor includes */
#include <tresor/module.h>

namespace Tresor {

	class Sb_initializer;
	class Sb_initializer_request;
	class Sb_initializer_channel;
}


class Tresor::Sb_initializer_request : public Module_request
{
	private:

		friend class Sb_initializer;
		friend class Sb_initializer_channel;

		Tree_level_index _vbd_max_lvl;
		Tree_degree _vbd_degree;
		Number_of_leaves _vbd_num_leaves;
		Tree_level_index _ft_max_lvl;
		Tree_degree _ft_degree;
		Number_of_leaves _ft_num_leaves;
		Tree_level_index _mt_max_lvl;
		Tree_degree _mt_degree;
		Number_of_leaves _mt_num_leaves;
		Pba_allocator &_pba_alloc;
		bool &_success;

	public:

		Sb_initializer_request(Module_id , Module_request_id , Tree_level_index , Tree_degree , Number_of_leaves ,
		                       Tree_level_index , Tree_degree , Number_of_leaves , Tree_level_index , Tree_degree ,
		                       Number_of_leaves , Pba_allocator &, bool &);

		void print(Output &out) const override { Genode::print(out, "init"); }
};


class Tresor::Sb_initializer_channel : public Module_channel
{
	private:

		friend class Sb_initializer;

		using Request = Sb_initializer_request;

		enum State {
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, SLOT_COMPLETE, COMPLETE,
			FT_REQUEST_COMPLETE, MT_REQUEST_COMPLETE,
			SYNC_REQUEST_COMPLETE,
			SYNC_REQUEST_IN_PROGRESS,
			SYNC_REQUEST_PENDING,
			TA_REQUEST_CREATE_KEY_COMPLETE,
			TA_REQUEST_ENCRYPT_KEY_COMPLETE,
			TA_REQUEST_SECURE_SB_COMPLETE,
			VBD_REQUEST_COMPLETE,
			VBD_REQUEST_IN_PROGRESS,
			VBD_REQUEST_PENDING,
			WRITE_REQUEST_COMPLETE, REQ_GENERATED
		};

		State _state { INACTIVE };
		Request *_req_ptr { };
		Superblock_index _sb_slot_index { 0 };
		Superblock _sb { };
		Block _encoded_blk { };
		Key _key_plain { };
		Key _key_cipher { };
		Hash _sb_hash { };
		Hash _dummy_hash { };
		Constructible<Tree_root> _vbd { };
		Constructible<Tree_root> _mt { };
		Constructible<Tree_root> _ft { };
		bool _generated_req_success { false };

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == COMPLETE; }

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void clean_data()
		{
			_sb = Superblock { };

			memset(&_key_plain,   0, sizeof(_key_plain));
			memset(&_key_cipher,  0, sizeof(_key_cipher));
			memset(&_sb_hash,     0, sizeof(_sb_hash));

			_vbd.destruct();
			_ft.destruct();
			_mt.destruct();
		}
};


class Tresor::Sb_initializer : public Module
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

	public:

		Sb_initializer();

		void execute(bool &) override;

};

#endif /* _TRESOR__SB_INITIALIZER_H_ */
