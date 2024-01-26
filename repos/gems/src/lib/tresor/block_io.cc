/*
 * \brief  Module for accessing the back-end block device
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;


bool Block_io_sync::execute(Vfs::Vfs_handle &file_handle)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, file_handle);
		_helper.state = SYNC;
		progress = true;
		break;

	case SYNC: _file->sync(SYNC_OK, FILE_ERR, progress); break;
	case SYNC_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}

bool Block_io_read::execute(Vfs::Vfs_handle &file_handle)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, file_handle);
		_helper.state = READ;
		progress = true;
		break;

	case READ: _file->read(READ_OK, FILE_ERR, _helper.attr.in_pba * BLOCK_SIZE, { (char *)&_helper.attr.out_block, BLOCK_SIZE }, progress); break;
	case READ_OK:

		_helper.mark_succeeded(progress);
		if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == _helper.attr.in_pba))
			log("block_io: ", *this, " hash ", hash(_helper.attr.out_block));
		break;

	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Block_io_write::execute(Vfs::Vfs_handle &file_handle)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, file_handle);
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, _helper.attr.in_pba * BLOCK_SIZE, { (char *)&_helper.attr.in_block, BLOCK_SIZE }, progress); break;
	case WRITE_OK:

		_helper.mark_succeeded(progress);
		if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == _helper.attr.in_pba))
			log("block_io: ", *this, " hash ", hash(_helper.attr.in_block));
		break;

	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


Block_io_request::Block_io_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                                   Physical_block_address pba, Block &blk, bool &success)
:
	Module_request(src_module_id, src_chan_id, BLOCK_IO), _type(type), _pba(pba), _blk(blk), _success(success)
{ }


char const *Block_io_request::type_to_string(Type type)
{
	switch (type) {
	case READ: return "read";
	case WRITE: return "write";
	case SYNC: return "sync";
	}
	ASSERT_NEVER_REACHED;
}


void Block_io_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("block io: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Block_io_channel::_mark_req_failed(bool &progress, Error_string str)
{
	error("request failed: failed to ", str);
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Block_io_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
	if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == req._pba)) {
		switch (req._type) {
		case Request::READ:
		case Request::WRITE:
			log("block_io: ", req.type_to_string(req._type), " pba ", req._pba, " hash ", hash(req._blk));
			break;
		default: break;
		}
	}
}


void Block_io_channel::_read(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _file.read(READ_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::_write(bool &progress)
{

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _file.write(WRITE_OK, FILE_ERR, req._pba * BLOCK_SIZE, { (char *)&req._blk, BLOCK_SIZE }, progress); break;
	case WRITE_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::_sync(bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED: _file.sync(SYNC_OK, FILE_ERR, progress); break;
	case SYNC_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Block_io_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::READ: _read(progress); break;
	case Request::WRITE: _write(progress); break;
	case Request::SYNC: _sync(progress); break;
	}
}


void Block_io::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


void Block_io_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Block_io_channel::Block_io_channel(Module_channel_id id, Vfs::Env &vfs_env, Xml_node const &xml_node)
:
	Module_channel(BLOCK_IO, id), _vfs_env(vfs_env), _path(xml_node.attribute_value("path", Tresor::Path()))
{ }


Block_io::Block_io(Vfs::Env &vfs_env, Xml_node const &xml_node, Vfs::Vfs_handle &file_handle)
:
	_vfs_env(vfs_env), _path(xml_node.attribute_value("path", Path())), _file_handle(file_handle)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, vfs_env, xml_node);
		add_channel(*chan);
	}
}
