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

/* base includes */
#include <base/log.h>

/* tresor includes */
#include <tresor/hash.h>
#include <tresor/block_io.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/trust_anchor.h>
#include <tresor/sb_initializer.h>

using namespace Tresor;

Sb_initializer_request::
Sb_initializer_request(Module_id src_mod, Module_request_id src_chan, Tree_level_index vbd_max_level_idx,
                       Tree_degree vbd_degree, Number_of_leaves vbd_nr_of_leaves, Tree_level_index ft_max_level_idx,
                       Tree_degree ft_degree, Number_of_leaves ft_nr_of_leaves, Tree_level_index mt_max_level_idx,
                       Tree_degree mt_degree, Number_of_leaves mt_nr_of_leaves, Pba_allocator &pba_alloc,  bool &success)
:
	Module_request { src_mod, src_chan, SB_INITIALIZER }, _vbd_max_level_idx { vbd_max_level_idx },
	_vbd_degree { vbd_degree }, _vbd_nr_of_leaves { vbd_nr_of_leaves }, _ft_max_level_idx { ft_max_level_idx },
	_ft_degree { ft_degree }, _ft_nr_of_leaves { ft_nr_of_leaves }, _mt_max_level_idx { mt_max_level_idx },
	_mt_degree { mt_degree }, _mt_nr_of_leaves { mt_nr_of_leaves }, _pba_alloc { pba_alloc }, _success { success }
{ }


void Sb_initializer::_populate_sb_slot(Channel &channel,
                                       Physical_block_address first,
                                       Number_of_blocks       num)
{
	Superblock &sb = channel._sb;
	Request &req { *channel._req_ptr };
	Type_1_node &vbd_node = channel._vbd_node;

	sb.state = Superblock::NORMAL;
	sb.snapshots.items[0] = Snapshot {
		.hash         = vbd_node.hash,
		.pba          = vbd_node.pba,
		.gen          = 0,
		.nr_of_leaves = req._vbd_nr_of_leaves,
		.max_level    = req._vbd_max_level_idx,
		.valid        = true,
		.id           = 0,
		.keep         = false
	};

	sb.rekeying_vba            = 0;
	sb.resizing_nr_of_pbas     = 0;
	sb.resizing_nr_of_leaves   = 0;
	memset(&sb.previous_key, 0, sizeof(sb.previous_key));
	sb.current_key             = channel._key_cipher;
	sb.curr_snap_idx           = 0;
	sb.degree                  = req._vbd_degree;
	sb.first_pba               = first;
	sb.nr_of_pbas              = num;
	sb.last_secured_generation = 0;
/*
	sb.free_number             = channel._ft->pba;
	sb.free_gen                = channel._ft->gen;
	sb.free_hash               = channel._ft->hash;
*/
	sb.free_max_level          = channel._ft->max_lvl;
	sb.free_degree             = channel._ft->degree;
	sb.free_leaves             = channel._ft->num_leaves;
/*
	sb.meta_number             = channel._mt->pba;
	sb.meta_gen                = channel._mt->gen;
	sb.meta_hash               = channel._mt->hash;
*/
	sb.meta_max_level          = channel._mt->max_lvl;
	sb.meta_degree             = channel._mt->degree;
	sb.meta_leaves             = channel._mt->num_leaves;
}


void Sb_initializer::_execute(Channel &channel,
                              bool    &progress)
{

	using CS = Channel::State;
	Superblock &sb { channel._sb };
	Request &req { *channel._req_ptr };

	switch (channel._state) {
	case CS::IN_PROGRESS:

		if (channel._sb_slot_index == 0) {
			channel._state = CS::VBD_REQUEST_PENDING;
		} else {
			channel._sb.encode_to_blk(channel._encoded_blk);
			channel._generate_req<Block_io::Write>(CS::WRITE_REQUEST_COMPLETE, progress, channel._sb_slot_index, channel._encoded_blk);
		}
		progress = true;
		break;

	case CS::VBD_REQUEST_COMPLETE:

		channel._ft.construct(sb.free_number, sb.free_gen, sb.free_hash, req._ft_max_level_idx, req._ft_degree, req._ft_nr_of_leaves);
		channel.generate_req<Ft_initializer_request>(CS::FT_REQUEST_COMPLETE, progress, *channel._ft, req._pba_alloc, channel._generated_req_success);
		channel._state = Channel::REQ_GENERATED;
		break;

	case CS::FT_REQUEST_COMPLETE:

		channel._mt.construct(sb.meta_number, sb.meta_gen, sb.meta_hash, req._ft_max_level_idx, req._ft_degree, req._ft_nr_of_leaves);
		channel.generate_req<Ft_initializer_request>(CS::MT_REQUEST_COMPLETE, progress, *channel._mt, req._pba_alloc, channel._generated_req_success);
		channel._state = Channel::REQ_GENERATED;
		break;

	case CS::MT_REQUEST_COMPLETE:

		channel.generate_req<Trust_anchor::Create_key>(
			CS::TA_REQUEST_CREATE_KEY_COMPLETE, progress, channel._key_plain.value, channel._generated_req_success);
		channel._state = Channel::REQ_GENERATED;
		break;

	case CS::TA_REQUEST_CREATE_KEY_COMPLETE:

		channel.generate_req<Trust_anchor::Encrypt_key>(
			CS::TA_REQUEST_ENCRYPT_KEY_COMPLETE, progress, channel._key_plain.value, channel._key_cipher.value, channel._generated_req_success);
		channel._state = Channel::REQ_GENERATED;
		break;

	case CS::TA_REQUEST_ENCRYPT_KEY_COMPLETE:

		channel._key_cipher.id = 1;
		_populate_sb_slot(channel,
			req._pba_alloc.first_pba() - NR_OF_SUPERBLOCK_SLOTS,
			req._pba_alloc.num_used_pbas() + NR_OF_SUPERBLOCK_SLOTS);

		channel._sb.encode_to_blk(channel._encoded_blk);
		calc_hash(channel._encoded_blk, channel._sb_hash);
		channel._generate_req<Block_io::Write>(CS::WRITE_REQUEST_COMPLETE, progress, channel._sb_slot_index, channel._encoded_blk);
		break;

	case CS::WRITE_REQUEST_COMPLETE:

		channel._generate_req<Block_io::Sync>(CS::SYNC_REQUEST_COMPLETE, progress);
		progress = true;
		break;

	case CS::SYNC_REQUEST_COMPLETE:

		if (channel._sb_slot_index == 0) {
			channel.generate_req<Trust_anchor::Write_hash>(
				CS::TA_REQUEST_SECURE_SB_COMPLETE, progress, channel._sb_hash, channel._generated_req_success);
			channel._state = Channel::REQ_GENERATED;
		} else {
			channel._state = CS::SLOT_COMPLETE;
		}
		progress = true;
		break;

	case CS::TA_REQUEST_SECURE_SB_COMPLETE:

		channel._state = CS::SLOT_COMPLETE;
		progress = true;
		break;
	default:
		break;
	}
}


void Sb_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		//_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Sb_initializer::_execute_init(Channel &channel,
                                   bool    &progress)
{
	switch (channel._state) {
	case Channel::SUBMITTED:

		channel._sb_slot_index = 0;
		channel._state = Channel::PENDING;
		progress = true;
		return;

	case Channel::PENDING:

		channel.clean_data();
		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::SLOT_COMPLETE:

		if (channel._sb_slot_index < NR_OF_SUPERBLOCK_SLOTS - 1) {
			++channel._sb_slot_index;
			channel._state = Channel::PENDING;
			progress = true;
		} else
			_mark_req_successful(channel, progress);
		return;

	case Channel::IN_PROGRESS:
	case Channel::FT_REQUEST_COMPLETE:
	case Channel::MT_REQUEST_COMPLETE:
	case Channel::VBD_REQUEST_COMPLETE:
	case Channel::SYNC_REQUEST_COMPLETE:
	case Channel::TA_REQUEST_CREATE_KEY_COMPLETE:
	case Channel::TA_REQUEST_ENCRYPT_KEY_COMPLETE:
	case Channel::TA_REQUEST_SECURE_SB_COMPLETE:
	case Channel::WRITE_REQUEST_COMPLETE: _execute(channel, progress); return;
	default: return;
	}
}


void Sb_initializer::_mark_req_failed(Channel    &channel,
                                       bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	channel._req_ptr->_success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Sb_initializer::_mark_req_successful(Channel &channel,
                                           bool    &progress)
{
	Request &req { *channel._req_ptr };

	req._success = true;

	channel._state = Channel::COMPLETE;
	progress = true;
}


bool Sb_initializer::_peek_completed_request(uint8_t *buf_ptr,
                                             size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(Request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &(*channel._req_ptr), sizeof(Request));
			return true;
		}
	}
	return false;
}


void Sb_initializer::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
	_channels[id]._req_ptr.destruct();
}


bool Sb_initializer::_peek_generated_request(uint8_t *buf_ptr,
                                             size_t   buf_size)
{
	using CS = Channel::State;

	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &channel { _channels[id] };
		if (channel._state == CS::INACTIVE)
			continue;

		switch (channel._state) {
		case CS::VBD_REQUEST_PENDING:
		{
			Vbd_initializer_request::Type const vbd_initializer_req_type {
				Vbd_initializer_request::INIT };

			Request &req { *channel._req_ptr };
			Vbd_initializer_request::create(
				buf_ptr, buf_size, SB_INITIALIZER, id,
				vbd_initializer_req_type,
				req._vbd_max_level_idx,
				req._vbd_degree - 1,
				req._vbd_nr_of_leaves, req._pba_alloc);

			return true;
		}
		default: break;
		}
	}
	return false;
}


void Sb_initializer::_drop_generated_request(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]._state) {
	case Channel::VBD_REQUEST_PENDING:
		_channels[id]._state = Channel::VBD_REQUEST_IN_PROGRESS;
		break;
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Sb_initializer::generated_request_complete(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &channel = _channels[id];

	switch (channel._state) {
	case Channel::VBD_REQUEST_IN_PROGRESS:
	{
		if (req.dst_module_id() != VBD_INITIALIZER) {
			class Exception_3 { };
			throw Exception_3 { };
		}
		Vbd_initializer_request const *vbd_initializer_req = static_cast<Vbd_initializer_request const*>(&req);
		channel._state = Channel::VBD_REQUEST_COMPLETE;
		channel._generated_req_success = vbd_initializer_req->success();
		memcpy(&channel._vbd_node,
		       const_cast<Vbd_initializer_request*>(vbd_initializer_req)->root_node(),
		       sizeof(Type_1_node));

		break;
	}
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


Sb_initializer::Sb_initializer()
{ register_channels(_channels, NR_OF_CHANNELS, SB_INITIALIZER); }


bool Sb_initializer::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}


void Sb_initializer::submit_request(Module_request &mod_req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			Request &req { *static_cast<Request*>(&mod_req) };
			req.dst_request_id(id);
			_channels[id]._req_ptr.construct(
				req.src_module_id(), req.src_chan_id(), req._vbd_max_level_idx,
				req._vbd_degree, req._vbd_nr_of_leaves, req._ft_max_level_idx, req._ft_degree,
				req._ft_nr_of_leaves, req._mt_max_level_idx, req._mt_degree,
				req._mt_nr_of_leaves, req._pba_alloc, req._success);
			_channels[id]._req_ptr->dst_request_id(id);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


void Sb_initializer::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		_execute_init(channel, progress);
	}
}
