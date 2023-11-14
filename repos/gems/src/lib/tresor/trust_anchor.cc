/*
 * \brief  Module for accessing the systems trust anchor
 * \author Martin Stein
 * \date   2023-02-13
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
#include <tresor/trust_anchor.h>

using namespace Tresor;

Trust_anchor_request::Trust_anchor_request(Module_id src_module_id, Module_channel_id src_chan_id,
                                           Type type, Key_value &key_plaintext, Key_value &key_ciphertext,
                                           Hash &hash, Passphrase passphrase, bool &success)
:
	Module_request { src_module_id, src_chan_id, TRUST_ANCHOR }, _type { type }, _key_plaintext { key_plaintext },
	_key_ciphertext { key_ciphertext }, _hash { hash }, _passphrase { passphrase }, _success { success }
{ }


char const *Trust_anchor_request::type_to_string(Type type)
{
	switch (type) {
	case CREATE_KEY: return "create_key";
	case ENCRYPT_KEY: return "encrypt_key";
	case DECRYPT_KEY: return "decrypt_key";
	case SECURE_SUPERBLOCK: return "secure_superblock";
	case GET_LAST_SB_HASH: return "get_last_sb_hash";
	case INITIALIZE: return "initialize";
	}
	ASSERT_NEVER_REACHED;
}


void Trust_anchor_channel::
_write_read_file(Vfs::Vfs_handle &file, char const *write_buf, char *read_buf, size_t read_size, bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case WRITE_PENDING:

		file.seek(_file_offset);
		_state = WRITE_IN_PROGRESS;
		progress = true;
		return;

	case WRITE_IN_PROGRESS:
	{
		size_t nr_of_written_bytes { 0 };
		Const_byte_range_ptr src { write_buf + _file_offset, _file_size };
		switch (file.fs().write(&file, src, nr_of_written_bytes)) {
		case Write_result::WRITE_ERR_WOULD_BLOCK: return;
		case Write_result::WRITE_OK:

			_file_offset += nr_of_written_bytes;
			_file_size -= nr_of_written_bytes;
			if (_file_size > 0) {
				_state = WRITE_PENDING;
				progress = true;
				return;
			}
			_state = READ_PENDING;
			_file_offset = 0;
			_file_size = read_size;
			progress = true;
			return;

		default:
			req._success = false;
			error("failed to write file");
			_state = REQ_COMPLETE;
			_req_ptr = nullptr;
			progress = true;
			return;
		}
	}
	case READ_PENDING:

		file.seek(_file_offset);
		if (!file.fs().queue_read(&file, _file_size))
			return;

		_state = READ_IN_PROGRESS;
		progress = true;
		return;

	case READ_IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };
		Byte_range_ptr dst { read_buf + _file_offset, _file_size };
		switch (file.fs().complete_read( &file, dst, nr_of_read_bytes)) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK: return;
		case Read_result::READ_OK:

			_file_offset += nr_of_read_bytes;
			_file_size -= nr_of_read_bytes;
			req._success = true;
			if (_file_size > 0) {
				_state = READ_PENDING;
				progress = true;
				return;
			}
			_state = REQ_COMPLETE;
			_req_ptr = nullptr;
			progress = true;
			return;

		default:
			req._success = false;
			error("failed to read file");
			_state = REQ_COMPLETE;
			_req_ptr = nullptr;
			return;
		}
	}
	default: return;
	}
}


void Trust_anchor_channel::_mark_req_failed(bool &progress, Error_string str)
{
	error("trust_anchor: request (", *_req_ptr, ") failed: ", str);
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Trust_anchor_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Trust_anchor_channel::_get_last_sb_hash(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _hashsum_file.read(READ_OK, FILE_ERR, 0, { (char *)&req._hash, HASH_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_create_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _generate_key_file.read(READ_OK, FILE_ERR, 0, { (char *)&req._key_plaintext, KEY_SIZE }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_initialize(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _initialize_file.write(WRITE_OK, FILE_ERR, 0, { req._passphrase.string(), req._passphrase.length() - 1 }, progress); break;
	case WRITE_OK: _initialize_file.read(READ_OK, FILE_ERR, 0, { _read_buf, sizeof(_read_buf) }, progress); break;
	case READ_OK:

		if (strcmp(_read_buf, "ok", 3))
			_mark_req_failed(progress, { "trust anchor did not return \"ok\""});
		else
			_mark_req_successful(progress);
		break;

	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::_secure_sb(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED: _hashsum_file.write(WRITE_OK, FILE_ERR, 0, { (char *)&req._hash, HASH_SIZE }, progress); break;
	case WRITE_OK: _hashsum_file.read(READ_OK, FILE_ERR, 0, { _read_buf, 0 }, progress); break;
	case READ_OK: _mark_req_successful(progress); break;
	case FILE_ERR: _mark_req_failed(progress, "file operation failed"); break;
	default: break;
	}
}


void Trust_anchor_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (req._type) {
	case Request::INITIALIZE: _initialize(progress); break;
	case Request::SECURE_SUPERBLOCK: _secure_sb(progress); break;
	case Request::GET_LAST_SB_HASH: _get_last_sb_hash(progress); break;
	case Request::CREATE_KEY: _create_key(progress); break;
	case Request::ENCRYPT_KEY:

		if (_state == REQ_SUBMITTED) {
			_state = WRITE_PENDING;
			_file_offset = 0;
			_file_size = sizeof(req._key_plaintext);
		}
		_write_read_file(
			_encrypt_file, (char const *)&req._key_plaintext, (char *)&req._key_ciphertext,
			sizeof(req._key_ciphertext), progress);
		break;

	case Request::DECRYPT_KEY:

		if (_state == REQ_SUBMITTED) {
			_state = WRITE_PENDING;
			_file_offset = 0;
			_file_size = sizeof(req._key_ciphertext);
		}
		_write_read_file(
			_decrypt_file, (char const *)&req._key_ciphertext, (char *)&req._key_plaintext,
			sizeof(req._key_plaintext), progress);
		break;
	}
}


Trust_anchor_channel::Trust_anchor_channel(Module_channel_id id, Vfs::Env &vfs_env, Xml_node const &xml_node)
:
	Module_channel { TRUST_ANCHOR, id }, _vfs_env { vfs_env }, _path { xml_node.attribute_value("path", Path()) }
{ }


Trust_anchor::Trust_anchor(Vfs::Env &vfs_env, Xml_node const &xml_node)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, vfs_env, xml_node);
		add_channel(*chan);
	}
}


void Trust_anchor_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


void Trust_anchor::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
