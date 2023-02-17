
/* base includes */
#include <base/log.h>

/* cbe tester includes */
#include <crypta.h>

using namespace Genode;
using namespace Cbe;


/*************************
 ** Cbe::Crypta_request **
 *************************/

void Cbe::Crypta_request::create(
	void     * buf_ptr,
	size_t     buf_size,
	size_t     req_type,
	uint64_t   /*req_blk_nr*/,
	void     * prim_ptr,
	size_t     prim_size,
	uint32_t   key_id,
	void     * key_plain_ptr,
	uint64_t   /*pba*/,
	uint64_t   /*vba*/,
	void     * /*plain_blk_ptr*/,
	void     * /*cipher_blk_ptr*/)
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
		memcpy(&req._key_plaintext, key_plain_ptr, sizeof(req._key_plaintext));
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

bool Cbe::Crypta::_peek_generated_request(uint8_t *,
                                          size_t   )
{
/*
	for (uint32_t idx { 0 }; idx < NR_OF_CHANNELS; idx++) {
		Channel &channel { _channels[idx] };
		if (channel._state == Channel::PENDING) {

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
		}
	}
*/
	return false;
}


void Cbe::Crypta::_drop_generated_request(Module_request &)
{
/*
	unsigned long id { 0 };
	switch (mod_req.dst_module_id()) {
	case CRYPTO:

		id = dynamic_cast<Crypto_request *>(&mod_req)->src_request_id();
		break;

	case CRYPTA:

		class Bad_module { };
		throw Bad_module { };
	}
	if (id >= NR_OF_CHANNELS) {

		class Bad_id { };
		throw Bad_id { };
	}
	if (_channels[id]._state != Channel::PENDING) {
*/
		class Bad_state { };
		throw Bad_state { };
/*
	}
	_channels[id]._state = Channel::IN_PROGRESS;
*/
}


Crypta::Key_directory &Crypta::_get_unused_key_dir()
{
	for (Key_directory &key_dir : _key_dirs) {
		if (key_dir.key_id == 0)
			return key_dir;
	}
	class Exception_1 { };
	throw Exception_1 { };
}


void Crypta::execute(bool &progress)
{
	for (Channel &channel : _channels) {
		if (channel._state != Channel::INACTIVE) {

			Request &req { channel._request };
			switch (req._type) {
			case Request::ADD_KEY:

				switch (channel._state) {
				case Channel::SUBMITTED:
				{
					_add_key_handle.seek(0);

					char buf[sizeof(req._key_plaintext) +
					         sizeof(req._key_id)] { };

					memcpy(buf, &req._key_id, sizeof(req._key_id));
					memcpy(buf + sizeof(req._key_id), req._key_plaintext,
					       sizeof(req._key_plaintext));

					Vfs::file_size nr_of_written_bytes { 0 };

					Write_result const write_result {
						_add_key_handle.fs().write(
							&_add_key_handle, buf, sizeof(buf),
							nr_of_written_bytes) };

					switch (write_result) {
					case Write_result::WRITE_OK:
						break;
					case Write_result::WRITE_ERR_WOULD_BLOCK:
					case Write_result::WRITE_ERR_INVALID:
					case Write_result::WRITE_ERR_IO:
						class Exception_1 { };
						throw Exception_1 { };
					}
					Key_directory &key_dir { _get_unused_key_dir() };
					key_dir.key_id = req._key_id;
					key_dir.encrypt_handle =
						&vfs_open_rw(
							_vfs_env, { _path.string(), "/keys/", req._key_id,
						                "/encrypt" });

					key_dir.decrypt_handle =
						&vfs_open_rw(
							_vfs_env, { _path.string(), "/keys/", req._key_id,
							            "/decrypt" });

					req._success = true;
					channel._state = Channel::COMPLETE;
					progress = true;
					break;
				}
				case Channel::COMPLETE:
				case Channel::INACTIVE:

					break;
				}
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
