
/* base includes */
#include <base/log.h>

/* cbe tester includes */
#include <crypta.h>
#include <client_data.h>

using namespace Genode;
using namespace Cbe;


/*************************
 ** Cbe::Crypta_request **
 *************************/

void Cbe::Crypta_request::create(
	void     * buf_ptr,
	size_t     buf_size,
	size_t     req_type,
	uint64_t   cbe_req_blk_nr,
	uint64_t   cbe_req_offset,
	uint64_t   cbe_req_tag,
	void     * prim_ptr,
	size_t     prim_size,
	uint32_t   key_id,
	void     * key_plaintext_ptr,
	uint64_t   pba,
	uint64_t   vba,
	void     * /*plaintext_blk_ptr*/,
	void     * ciphertext_blk_ptr)
{
	Crypta_request req { CBE_LIBRARA, ~0UL };
	switch (req_type) {
	case ADD_KEY:

		req._type = ADD_KEY;
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Bad_size_1 { };
			throw Bad_size_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
		req._key_id = key_id;
		memcpy(&req._key_plaintext, key_plaintext_ptr, sizeof(req._key_plaintext));
		break;

	case ENCRYPT_CLIENT_DATA:

		req._type = ENCRYPT_CLIENT_DATA;
		req._cbe_req_blk_nr = cbe_req_blk_nr;
		req._cbe_req_offset = cbe_req_offset;
		req._cbe_req_tag = cbe_req_tag;
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Bad_size_1 { };
			throw Bad_size_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
		req._key_id = key_id;
		req._pba = pba;
		req._vba = vba;
		req._ciphertext_blk_ptr = (addr_t)ciphertext_blk_ptr;
		break;
/*
	case REMOVE_KEY:

		req._type = REMOVE_KEY;
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Bad_size_4 { };
			throw Bad_size_4 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);

		if (key_id_size != sizeof(req._key_id)) {
			error(key_id_size, " ", sizeof(req._key_id));
			class Bad_size_5 { };
			throw Bad_size_5 { };
		}
		memcpy(&req._key_id, key_id_ptr, key_id_size);
		break;
*/
	default:

		error("Bad crypta request type: ", req_type);
		class Bad_type { };
		throw Bad_type { };
	}
	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


/*****************
 ** Cbe::Crypta **
 *****************/

bool Cbe::Crypta::_peek_generated_request(uint8_t *buf_ptr,
                                          size_t   buf_size)
{
	for (uint32_t idx { 0 }; idx < NR_OF_CHANNELS; idx++) {
		Channel &channel { _channels[idx] };
		if (channel._state == Channel::OBTAIN_PLAINTEXT_BLK_PENDING) {

			Client_data_request const req {
				CRYPTA, idx, Client_data_request::OBTAIN_PLAINTEXT_BLK,
				channel._request._cbe_req_blk_nr, channel._request._cbe_req_offset,
				channel._request._cbe_req_tag, channel._request._vba,
				(addr_t)&channel._plaintext_blk };

			if (sizeof(req) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &req, sizeof(req));;
			return true;
/*
			switch (channel._request._type) {
			case Request::ADD_KEY:
			{
				Key key;
				if (sizeof(key.value) != sizeof(channel._request._key_plaintext)) {
					class Bad_size_1 { };
					throw Bad_size_1 { };
				}
				memcpy(key.value, channel._request._key_plaintext, sizeof(key.value));
				key.id.value = channel._request._key_id;
				Cbe::Request cbe_req { Cbe::Request::Operation::READ, false, 0, 0, 1, 0, idx };
				Crypto_request req { CRYPTA, idx, Crypto_request::ADD_KEY, cbe_req, key };

				if (sizeof(req) > buf_size) {
					class Bad_size_2 { };
					throw Bad_size_2 { };
				}
				memcpy(buf_ptr, &req, sizeof(req));;
				return true;
			}
			case Request::REMOVE_KEY:
			{
				Key key;
				key.id.value = channel._request._key_id;
				Cbe::Request cbe_req { Cbe::Request::Operation::READ, false, 0, 0, 1, 0, idx };
				Crypto_request req { CRYPTA, idx, Crypto_request::REMOVE_KEY, cbe_req, key };

				if (sizeof(req) > buf_size) {
					class Bad_size_2 { };
					throw Bad_size_2 { };
				}
				memcpy(buf_ptr, &req, sizeof(req));;
				return true;
			}
			default:
				class Bad_type { };
				throw Bad_type { };
			}
*/
		}
	}
	return false;
}


void Cbe::Crypta::_drop_generated_request(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]._state) {
	case Channel::OBTAIN_PLAINTEXT_BLK_PENDING:
		_channels[id]._state = Channel::OBTAIN_PLAINTEXT_BLK_IN_PROGRESS;
		break;
	default:
		class Bad_state { };
		throw Bad_state { };
	}
}


Crypta::Key_directory &Crypta::_lookup_key_dir(uint32_t key_id)
{
	for (Key_directory &key_dir : _key_dirs) {
		if (key_dir.key_id == key_id) {
			return key_dir;
		}
	}
	class Exception_1 { };
	throw Exception_1 { };
}

void Crypta::_mark_req_failed(Channel    &channel,
                              bool       &progress,
                              char const *str)
{
	error("request failed: ", str);
	channel._request._success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Crypta::_mark_req_successful(Channel &channel,
                                  bool    &progress)
{
	channel._request._success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Crypta::_execute_add_key(Channel &channel,
                              bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:
	{
		_add_key_handle.seek(0);

		char buf[sizeof(req._key_plaintext) + sizeof(req._key_id)] { };
		memcpy(buf, &req._key_id, sizeof(req._key_id));
		memcpy(buf + sizeof(req._key_id), req._key_plaintext,
		       sizeof(req._key_plaintext));

		Vfs::file_size nr_of_written_bytes { 0 };
		Write_result const write_result {
			_add_key_handle.fs().write(
				&_add_key_handle, buf, sizeof(buf), nr_of_written_bytes) };

		switch (write_result) {
		case Write_result::WRITE_OK:
		{
			Key_directory *key_dir_ptr { nullptr };
			for (Key_directory &key_dir : _key_dirs) {
				if (key_dir.key_id == 0)
					key_dir_ptr = &key_dir;
			}
			if (key_dir_ptr == nullptr) {

				_mark_req_failed(channel, progress, "no key dir");
				return;
			}
			key_dir_ptr->key_id = req._key_id;
			key_dir_ptr->encrypt_handle =
				&vfs_open_rw(
					_vfs_env,
					{ _path.string(), "/keys/", req._key_id, "/encrypt" });

			key_dir_ptr->decrypt_handle =
				&vfs_open_rw(
					_vfs_env,
					{ _path.string(), "/keys/", req._key_id, "/decrypt" });

			_mark_req_successful(channel, progress);
			return;
		}
		case Write_result::WRITE_ERR_WOULD_BLOCK:
		case Write_result::WRITE_ERR_INVALID:
		case Write_result::WRITE_ERR_IO:

			_mark_req_failed(channel, progress, "vfs write error");
			return;
		}
		return;
	}
	default:

		return;
	}
}


void Crypta::_execute_encrypt_client_data(Channel &channel,
                                          bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:

		channel._state = Channel::OBTAIN_PLAINTEXT_BLK_PENDING;
		progress = true;
		return;

	case Channel::OBTAIN_PLAINTEXT_BLK_COMPLETE:
	{
		if (!channel._generated_req_success) {

			_mark_req_failed(channel, progress, "no plaintext block");
			return;
		}
		channel._vfs_handle = _lookup_key_dir(req._key_id).encrypt_handle;
		channel._vfs_handle->seek(req._cbe_req_blk_nr * Cbe::BLOCK_SIZE);
		Vfs::file_size nr_of_written_bytes { 0 };

		channel._vfs_handle->fs().write(
			channel._vfs_handle, (char *)&channel._plaintext_blk,
			Cbe::BLOCK_SIZE, nr_of_written_bytes);

		channel._state = Channel::OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case Channel::OP_WRITTEN_TO_VFS_HANDLE:
	{
		channel._vfs_handle->seek(req._cbe_req_blk_nr * Cbe::BLOCK_SIZE);
		bool success {
			channel._vfs_handle->fs().queue_read(
				channel._vfs_handle, Cbe::BLOCK_SIZE) };

		if (!success)
			return;

		channel._state = Channel::QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case Channel::QUEUE_READ_SUCCEEDED:
	{
		Vfs::file_size nr_of_read_bytes { 0 };
		Read_result const result {
			channel._vfs_handle->fs().complete_read(
				channel._vfs_handle, (char *)req._ciphertext_blk_ptr,
				Cbe::BLOCK_SIZE, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			_mark_req_successful(channel, progress);
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(channel, progress, "vfs read error");
			return;
		}
	}
	default:

		return;
	}
}


void Crypta::execute(bool &progress)
{
	for (Channel &channel : _channels) {
		if (channel._state != Channel::INACTIVE) {

			switch (channel._request._type) {
			case Request::ADD_KEY:

				_execute_add_key(channel, progress);
				break;

			case Request::ENCRYPT_CLIENT_DATA:

				_execute_encrypt_client_data(channel, progress);
				break;

			default:

				class Bad_request_type { };
				throw Bad_request_type { };
			}
		}
	}
}

Crypta::Crypta(Vfs::Env       &vfs_env,
               Xml_node const &xml_node)
:
	_vfs_env           { vfs_env },
	_path              { xml_node.attribute_value("path", String<32>()) },
	_add_key_handle    { vfs_open_wo(_vfs_env, { _path.string(), "/add_key" }) }
{
	for (Channel &channel : _channels)
		channel = Channel { };
}

void Crypta::generated_request_complete(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]._state) {
	case Channel::OBTAIN_PLAINTEXT_BLK_IN_PROGRESS:
		_channels[id]._state = Channel::OBTAIN_PLAINTEXT_BLK_COMPLETE;
		_channels[id]._generated_req_success =
			dynamic_cast<Client_data_request *>(&req)->success();
		break;
	default:
		class Bad_state { };
		throw Bad_state { };
	}
}
