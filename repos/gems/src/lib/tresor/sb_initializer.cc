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


Sb_initializer_request::Sb_initializer_request(Module_id         src_module_id,
                                                 Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, SB_INITIALIZER }
{ }


void Sb_initializer_request::create(void             *buf_ptr,
                                    size_t            buf_size,
                                    uint64_t          src_module_id,
                                    uint64_t          src_request_id,
                                    size_t            req_type,
                                    Tree_level_index  vbd_max_level_idx,
                                    Tree_degree       vbd_degree,
                                    Number_of_leaves  vbd_nr_of_leaves,
                                    Tree_level_index  ft_max_level_idx,
                                    Tree_degree       ft_degree,
                                    Number_of_leaves  ft_nr_of_leaves,
                                    Tree_level_index  mt_max_level_idx,
                                    Tree_degree       mt_degree,
                                    Number_of_leaves  mt_nr_of_leaves,
                                    Pba_allocator &pba_alloc)
{
	Sb_initializer_request req { src_module_id, src_request_id };

	req._type              = (Type)req_type;
	req._vbd_max_level_idx = vbd_max_level_idx;
	req._vbd_degree = vbd_degree;
	req._vbd_nr_of_leaves  = vbd_nr_of_leaves;
	req._ft_max_level_idx  = ft_max_level_idx;
	req._ft_degree  = ft_degree;
	req._ft_nr_of_leaves   = ft_nr_of_leaves;
	req._mt_max_level_idx  = mt_max_level_idx;
	req._mt_degree  = mt_degree;
	req._mt_nr_of_leaves   = mt_nr_of_leaves;
	req._pba_alloc_ptr   = (addr_t)&pba_alloc;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


char const *Sb_initializer_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case INIT:    return "init";
	}
	return "?";
}


void Sb_initializer::_populate_sb_slot(Channel &channel,
                                       Physical_block_address first,
                                       Number_of_blocks       num)
{
	Superblock &sb = channel._sb;

	Request     const &req      = channel._request;
	Type_1_node const &vbd_node = channel._vbd_node;

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

	switch (channel._state) {
	case CS::IN_PROGRESS:

		if (channel._sb_slot_index == 0) {
			channel._state = CS::VBD_REQUEST_PENDING;
		} else {
			channel._sb.encode_to_blk(channel._encoded_blk);
			channel._state = CS::WRITE_REQUEST_PENDING;
		}
		progress = true;
		break;

	case CS::VBD_REQUEST_COMPLETE:

		channel._state = CS::FT_REQUEST_PENDING;
		progress = true;
		break;

	case CS::FT_REQUEST_COMPLETE:

		channel._state = CS::MT_REQUEST_PENDING;
		progress = true;
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
			channel._request._pba_alloc().first_pba() - NR_OF_SUPERBLOCK_SLOTS,
			channel._request._pba_alloc().num_used_pbas() + NR_OF_SUPERBLOCK_SLOTS);

		channel._sb.encode_to_blk(channel._encoded_blk);
		calc_hash(channel._encoded_blk, channel._sb_hash);

		channel._state = CS::WRITE_REQUEST_PENDING;
		progress = true;
		break;

	case CS::WRITE_REQUEST_COMPLETE:

		channel._state = CS::SYNC_REQUEST_PENDING;
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

	/* finished */
	if (channel._sb_slot_index == NR_OF_SUPERBLOCK_SLOTS)
		_mark_req_successful(channel, progress);
}


void Sb_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", _request, ") failed because generated request failed)");
		_request._success = false;
		_state = COMPLETE;
		//_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Sb_initializer::_execute_init(Channel &channel,
                                   bool    &progress)
{
	using CS = Channel::State;

	switch (channel._state) {
	case CS::SUBMITTED:

		/*
		 * Reset the index on every new job as it is
		 * indicator for a finished job.
		 */
		channel._sb_slot_index = 0;

		channel._state = Channel::PENDING;
		progress = true;
		return;

	case CS::PENDING:

		/*
		 * Remove residual data here as we will end up
		 * here for every SB slot.
		 */
		channel.clean_data();

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case CS::IN_PROGRESS:

		_execute(channel, progress);
		return;

	case CS::SLOT_COMPLETE:

		if (channel._sb_slot_index < NR_OF_SUPERBLOCK_SLOTS) {
			++channel._sb_slot_index;
			channel._state = Channel::PENDING;
			progress = true;
		}
		return;

	case CS::FT_REQUEST_COMPLETE:

		_execute(channel, progress);
		return;

	case CS::MT_REQUEST_COMPLETE:

		_execute(channel, progress);
		return;

	case CS::VBD_REQUEST_COMPLETE:

		_execute(channel, progress);
		return;

	case CS::SYNC_REQUEST_COMPLETE:

		_execute(channel, progress);
		return;

	case CS::TA_REQUEST_CREATE_KEY_COMPLETE:

		_execute(channel, progress);
		return;

	case CS::TA_REQUEST_ENCRYPT_KEY_COMPLETE:

		_execute(channel, progress);
		return;

	case CS::TA_REQUEST_SECURE_SB_COMPLETE:

		_execute(channel, progress);
		return;

	case CS::WRITE_REQUEST_COMPLETE:

		_execute(channel, progress);
		return;

	default:
		/*
		 * Omit other states related to FT/MT/VBD as
		 * those are handled via Module API.
		 */
		return;
	}
}


void Sb_initializer::_mark_req_failed(Channel    &channel,
                                       bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	channel._request._success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Sb_initializer::_mark_req_successful(Channel &channel,
                                           bool    &progress)
{
	Request &req { channel._request };

	req._success = true;

	channel._state = Channel::COMPLETE;
	progress = true;
}


bool Sb_initializer::_peek_completed_request(uint8_t *buf_ptr,
                                             size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));
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
}


bool Sb_initializer::_peek_generated_request(uint8_t *buf_ptr,
                                             size_t   buf_size)
{
	using CS = Channel::State;

	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &channel { _channels[id] };
		Request &req { channel._request };
		Superblock &sb { channel._sb };

		if (channel._state == CS::INACTIVE)
			continue;

		switch (channel._state) {
		case CS::VBD_REQUEST_PENDING:
		{
			Vbd_initializer_request::Type const vbd_initializer_req_type {
				Vbd_initializer_request::INIT };

			Vbd_initializer_request::create(
				buf_ptr, buf_size, SB_INITIALIZER, id,
				vbd_initializer_req_type,
				channel._request._vbd_max_level_idx,
				channel._request._vbd_degree - 1,
				channel._request._vbd_nr_of_leaves, channel._request._pba_alloc());

			return true;
		}
		case CS::FT_REQUEST_PENDING:
		{
			ASSERT(sizeof(Ft_initializer_request) <= buf_size);
			channel._ft.construct(sb.free_number, sb.free_gen, sb.free_hash, req._ft_max_level_idx, req._ft_degree, req._ft_nr_of_leaves);
			construct_at<Ft_initializer_request>(buf_ptr, SB_INITIALIZER, id, *channel._ft, channel._request._pba_alloc(), channel._generated_req_success);
			return true;
		}
		case CS::MT_REQUEST_PENDING:
		{
			channel._mt.construct(sb.meta_number, sb.meta_gen, sb.meta_hash, req._ft_max_level_idx, req._ft_degree, req._ft_nr_of_leaves);
			construct_at<Ft_initializer_request>(buf_ptr, SB_INITIALIZER, id, *channel._mt, channel._request._pba_alloc(), channel._generated_req_success);
			return true;
		}
		case CS::WRITE_REQUEST_PENDING:
		{
			Block_io_request::Type const block_io_req_type {
				Block_io_request::WRITE };

			ASSERT(sizeof(Block_io_request) <= buf_size);
			construct_at<Block_io_request>(
				buf_ptr, SB_INITIALIZER, id, block_io_req_type, 0, 0,
				0, channel._sb_slot_index, 0, 1, channel._encoded_blk,
				channel._dummy_hash, channel._generated_req_success);

			return true;
		}
		case CS::SYNC_REQUEST_PENDING:
		{
			Block_io_request::Type const block_io_req_type {
				Block_io_request::SYNC };

			ASSERT(sizeof(Block_io_request) <= buf_size);
			construct_at<Block_io_request>(
				buf_ptr, SB_INITIALIZER, id,
				block_io_req_type, 0, 0, 0,
				channel._sb_slot_index, 0,
				0, channel._encoded_blk, channel._dummy_hash, channel._generated_req_success);

			return true;
		}
		default:
			break;
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
	case Channel::FT_REQUEST_PENDING:
		_channels[id]._state = Channel::FT_REQUEST_IN_PROGRESS;
		break;
	case Channel::MT_REQUEST_PENDING:
		_channels[id]._state = Channel::MT_REQUEST_IN_PROGRESS;
		break;
	case Channel::WRITE_REQUEST_PENDING:
		_channels[id]._state = Channel::WRITE_REQUEST_IN_PROGRESS;
		break;
	case Channel::SYNC_REQUEST_PENDING:
		_channels[id]._state = Channel::SYNC_REQUEST_IN_PROGRESS;
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
	case Channel::FT_REQUEST_IN_PROGRESS:
	{
		if (req.dst_module_id() != FT_INITIALIZER) {
			class Exception_4 { };
			throw Exception_4 { };
		}
		channel._state = Channel::FT_REQUEST_COMPLETE;
		break;
	}
	case Channel::MT_REQUEST_IN_PROGRESS:
	{
		if (req.dst_module_id() != FT_INITIALIZER) {
			class Exception_5 { };
			throw Exception_5 { };
		}
		channel._state = Channel::MT_REQUEST_COMPLETE;
		break;
	}
	case Channel::WRITE_REQUEST_IN_PROGRESS:
	{
		if (req.dst_module_id() != BLOCK_IO) {
			class Exception_9 { };
			throw Exception_9 { };
		}
		channel._state = Channel::WRITE_REQUEST_COMPLETE;
		break;
	}
	case Channel::SYNC_REQUEST_IN_PROGRESS:
	{
		if (req.dst_module_id() != BLOCK_IO) {
			class Exception_10 { };
			throw Exception_10 { };
		}
		channel._state = Channel::SYNC_REQUEST_COMPLETE;
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


void Sb_initializer::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *static_cast<Request *>(&req);
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

		Request &req { channel._request };
		switch (req._type) {
		case Request::INIT:

			_execute_init(channel, progress);

			break;
		default:

			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}
