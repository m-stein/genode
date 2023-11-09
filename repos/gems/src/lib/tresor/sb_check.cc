/*
 * \brief  Module for checking all hashes of a superblock and its hash trees
 * \author Martin Stein
 * \date   2023-05-03
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
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/block_io.h>

using namespace Tresor;

Sb_check_request::Sb_check_request(Module_id         src_module_id,
                                   Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, SB_CHECK }
{ }


Sb_check_request::Sb_check_request(Module_id src_mod, Module_request_id src_chan, bool &success)
:
	Module_request { src_mod, src_chan, SB_CHECK }, _success_ptr { (addr_t)&success }
{ }


void Sb_check_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_gen_prim_success) {
		error("sb check: request (", _request, ") failed because generated request failed)");
		_request._success() = false;
		_sb_slot_state = DONE;
		return;
	}
	_sb_slot_state = (Sb_slot_state)state_uint;
}


bool Sb_check::_handle_failed_generated_req(Channel &chan,
                                            bool    &progress)
{
	if (chan._gen_prim_success)
		return false;

	_mark_req_failed(chan, progress, "?");
	return true;
}


void Sb_check::_execute_check(Channel &chan,
                              bool    &progress)
{
	switch (chan._state) {
	case Channel::INSPECT_SBS:

		switch (chan._sb_slot_state) {
		case Channel::INIT:

			chan._gen_prim_blk_nr = chan._sb_slot_idx;
			chan._generate_req<Block_io::Read>(Channel::READ_DONE, progress, chan._gen_prim_blk_nr, chan._encoded_blk);
			break;

		case Channel::READ_DONE:
		{
			chan._sb_slot.decode_from_blk(chan._encoded_blk);
			Snapshot &snap { chan._sb_slot.curr_snap() };
			if (chan._sb_slot.valid() &&
			    snap.gen > chan._highest_gen) {

				chan._highest_gen = snap.gen;
				chan._last_sb_slot_idx = chan._sb_slot_idx;
			}
			if (chan._sb_slot_idx < MAX_SUPERBLOCK_INDEX) {

				chan._sb_slot_idx++;
				chan._sb_slot_state = Channel::INIT;
				progress = true;

			} else {

				chan._state = Channel::CHECK_SB;
				chan._sb_slot_idx = chan._last_sb_slot_idx;
				chan._sb_slot_state = Channel::INIT;
				progress = true;

				if (VERBOSE_CHECK)
					log("check superblock ", chan._sb_slot_idx);
			}
			break;
		}
		default:

			break;
		}
		break;

	case Channel::CHECK_SB:

		switch (chan._sb_slot_state) {
		case Channel::INIT:

			chan._gen_prim_blk_nr = chan._sb_slot_idx;
			chan._generate_req<Block_io::Read>(Channel::READ_DONE, progress, chan._gen_prim_blk_nr, chan._encoded_blk);
			if (VERBOSE_CHECK)
				log("  read superblock");

			break;

		case Channel::READ_DONE:

			chan._sb_slot.decode_from_blk(chan._encoded_blk);
			if (chan._sb_slot.valid()) {

				Snapshot &snap {
					chan._sb_slot.snapshots.items[chan._snap_idx] };

				if (snap.valid) {


					Snapshot &snap { chan._sb_slot.snapshots.items[chan._snap_idx] };
					chan._gen_prim_blk_nr = snap.pba;
					chan._generate_req<Vbd_check_request>(
						Channel::VBD_CHECK_DONE, progress, snap.max_level, chan._sb_slot.degree - 1,
						snap.nr_of_leaves, Type_1_node { snap.pba, snap.gen, snap.hash });
					if (VERBOSE_CHECK)
						log("  check snap ", chan._snap_idx, " (", snap, ")");

				} else {

					chan._sb_slot_state = Channel::VBD_CHECK_DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log("  skip snap ", chan._snap_idx,
						    " as it is unused");
				}
			} else {

				chan._sb_slot_state = Channel::DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log("  skip superblock as it is unused");
			}
			break;

		case Channel::VBD_CHECK_DONE:

			if (chan._snap_idx < MAX_SNAP_IDX) {

				chan._snap_idx++;
				chan._sb_slot_state = Channel::READ_DONE;
				progress = true;

			} else {

				chan._snap_idx = 0;
				chan._gen_prim_blk_nr = chan._sb_slot.free_number;
				chan._sb_slot_state = Channel::FT_CHECK_STARTED;
				progress = true;

				if (VERBOSE_CHECK)
					log("  check free tree");
			}
			break;

		case Channel::FT_CHECK_DONE:

			if (_handle_failed_generated_req(chan, progress))
				break;

			chan._sb_slot_state = Channel::MT_CHECK_STARTED;
			progress = true;

			if (VERBOSE_CHECK)
				log("  check meta tree");

			break;

		case Channel::MT_CHECK_DONE:

			if (_handle_failed_generated_req(chan, progress))
				break;

			_mark_req_successful(chan, progress);
			break;

		case Channel::DONE:

			break;

		default:

			break;
		}
		break;

	default:

		break;
	}
}


void Sb_check::_mark_req_failed(Channel    &chan,
                                bool       &progress,
                                char const *str)
{
	error("sb check: request (", chan._request, ") failed at step \"", str, "\"");
	chan._request._success() = false;
	chan._sb_slot_state = Channel::DONE;
	progress = true;
}


void Sb_check::_mark_req_successful(Channel &chan,
                                    bool    &progress)
{
	Request &req { chan._request };
	req._success() = true;
	chan._sb_slot_state = Channel::DONE;
	progress = true;
}


bool Sb_check::_peek_completed_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._sb_slot_state == Channel::DONE) {
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


void Sb_check::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._sb_slot_state != Channel::DONE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._sb_slot_state = Channel::INACTIVE;
}


bool Sb_check::_peek_generated_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &chan { _channels[id] };

		if (chan._sb_slot_state == Channel::INACTIVE)
			continue;

		switch (chan._sb_slot_state) {
		case Channel::FT_CHECK_STARTED:

			construct_in_buf<Ft_check_request>(
				buf_ptr, buf_size, SB_CHECK, id,
				Ft_check_request::CHECK,
				(Tree_level_index)chan._sb_slot.free_max_level,
				(Tree_degree)chan._sb_slot.free_degree - 1,
				(Number_of_leaves)chan._sb_slot.free_leaves,
				Type_1_node {
					chan._sb_slot.free_number,
					chan._sb_slot.free_gen,
					chan._sb_slot.free_hash });

			return true;

		case Channel::MT_CHECK_STARTED:

			construct_in_buf<Ft_check_request>(
				buf_ptr, buf_size, SB_CHECK, id,
				Ft_check_request::CHECK,
				(Tree_level_index)chan._sb_slot.meta_max_level,
				(Tree_degree)chan._sb_slot.meta_degree - 1,
				(Number_of_leaves)chan._sb_slot.meta_leaves,
				Type_1_node {
					chan._sb_slot.meta_number,
					chan._sb_slot.meta_gen,
					chan._sb_slot.meta_hash });

			return true;

		default:
			break;
		}
	}
	return false;
}


void Sb_check::_drop_generated_request(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_0 { };
		throw Exception_0 { };
	}
	Channel &chan { _channels[id] };
	switch (chan._sb_slot_state) {
	case Channel::FT_CHECK_STARTED: chan._sb_slot_state = Channel::FT_CHECK_DROPPED; break;
	case Channel::MT_CHECK_STARTED: chan._sb_slot_state = Channel::MT_CHECK_DROPPED; break;
	default:
		class Exception_4 { };
		throw Exception_4 { };
	}
}


void Sb_check::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case FT_CHECK:
	{
		Ft_check_request &gen_req { *static_cast<Ft_check_request*>(&mod_req) };
		chan._gen_prim_success = gen_req.success();
		switch (chan._sb_slot_state) {
		case Channel::FT_CHECK_DROPPED: chan._sb_slot_state = Channel::FT_CHECK_DONE; break;
		case Channel::MT_CHECK_DROPPED: chan._sb_slot_state = Channel::MT_CHECK_DONE; break;
		default:
			class Exception_3 { };
			throw Exception_3 { };
		}
		break;
	}
	default:
		class Exception_8 { };
		throw Exception_8 { };
	}
}


bool Sb_check::ready_to_submit_request()
{
	for (Channel &chan : _channels) {
		if (chan._sb_slot_state == Channel::INACTIVE)
			return true;
	}
	return false;
}


void Sb_check_channel::_reset()
{
	_state = INSPECT_SBS;
	_request = { };
	_highest_gen = 0;
	_last_sb_slot_idx = 0;
	_sb_slot_state = INACTIVE;
	_sb_slot_idx = 0;
	_sb_slot = { };
	_snap_idx = 0;
	_vbd = { };
	_ft = { };
	_mt = { };
	_gen_prim_blk_nr = 0;
	_gen_prim_success = false;
	_encoded_blk = { };
}


void Sb_check::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		Channel &chan { _channels[id] };
		if (chan._sb_slot_state == Channel::INACTIVE) {
			req.dst_request_id(id);
			chan._reset();
			chan._request = *static_cast<Request *>(&req);
			chan._sb_slot_state = Channel::INIT;
			return;
		}
	}
	class Exception_1 { };
	throw Exception_1 { };
}


void Sb_check::execute(bool &progress)
{
	for (Channel &chan : _channels) {

		if (chan._sb_slot_state == Channel::INACTIVE)
			continue;

		_execute_check(chan, progress);
	}
}
