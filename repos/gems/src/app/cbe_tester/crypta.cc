
/* base includes */
#include <base/log.h>

/* local includes */
#include <crypta.h>
#include <crypto.h>

using namespace Genode;
using namespace Cbe;


/*************************
 ** Cbe::Crypta_request **
 *************************/

void Cbe::Crypta_request::create(
	void     *buf_ptr,
	size_t    buf_size,
	size_t    req_type,
	void     *prim_ptr,
	size_t    prim_size,
	void     *key_id_ptr,
	size_t    key_id_size,
	void     *key_plaintext_ptr,
	size_t    key_plaintext_size,
	uint64_t  blk_nr)
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
		Genode::memcpy(&req._prim, prim_ptr, prim_size);

		if (key_id_size != sizeof(req._key_id)) {
			error(key_id_size, " ", sizeof(req._key_id));
			class Bad_size_2 { };
			throw Bad_size_2 { };
		}
		Genode::memcpy(&req._key_id, key_id_ptr, key_id_size);

		if (key_plaintext_size != sizeof(req._key_plaintext)) {
			error(key_plaintext_size, " ", sizeof(req._key_plaintext));
			class Bad_size_3 { };
			throw Bad_size_3 { };
		}
		Genode::memcpy(&req._key_plaintext, key_plaintext_ptr, key_plaintext_size);
		break;

	case REMOVE_KEY:

		req._type = REMOVE_KEY;
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Bad_size_4 { };
			throw Bad_size_4 { };
		}
		Genode::memcpy(&req._prim, prim_ptr, prim_size);

		if (key_id_size != sizeof(req._key_id)) {
			error(key_id_size, " ", sizeof(req._key_id));
			class Bad_size_5 { };
			throw Bad_size_5 { };
		}
		Genode::memcpy(&req._key_id, key_id_ptr, key_id_size);
		break;

	case DECRYPT_BLOCK:

		req._type = DECRYPT_BLOCK;
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Bad_size_4 { };
			throw Bad_size_4 { };
		}
		Genode::memcpy(&req._prim, prim_ptr, prim_size);

		if (key_id_size != sizeof(req._key_id)) {
			error(key_id_size, " ", sizeof(req._key_id));
			class Bad_size_5 { };
			throw Bad_size_5 { };
		}
		Genode::memcpy(&req._key_id, key_id_ptr, key_id_size);

		req._blk_nr = blk_nr;
		break;

	case ENCRYPT_BLOCK:

		req._type = ENCRYPT_BLOCK;
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Bad_size_4 { };
			throw Bad_size_4 { };
		}
		Genode::memcpy(&req._prim, prim_ptr, prim_size);

		if (key_id_size != sizeof(req._key_id)) {
			error(key_id_size, " ", sizeof(req._key_id));
			class Bad_size_5 { };
			throw Bad_size_5 { };
		}
		Genode::memcpy(&req._key_id, key_id_ptr, key_id_size);

		req._blk_nr = blk_nr;
		break;

	default:

		class Bad_type { };
		throw Bad_type { };
	}
	if (sizeof(req) > buf_size) {
		error(sizeof(req), " ", buf_size);
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


/*****************
 ** Cbe::Crypta **
 *****************/

bool Cbe::Crypta::_peek_generated_request(Genode::uint8_t *buf_ptr,
                                          Genode::size_t   buf_size)
{
	for (Genode::uint32_t idx { 0 }; idx < NR_OF_CHANNELS; idx++) {
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
				Genode::memcpy(key.value, channel._request._key_plaintext, sizeof(key.value));
				key.id.value = channel._request._key_id;
				Cbe::Request cbe_req { Cbe::Request::Operation::READ, false, 0, 0, 1, 0, idx };
				Crypto_request req {
					CRYPTA, idx, Crypto_request::ADD_KEY, cbe_req, key,
					(addr_t)&channel._plain_data_blk,
					(addr_t)&channel._cipher_data_blk };

				if (sizeof(req) > buf_size) {
					class Bad_size_2 { };
					throw Bad_size_2 { };
				}
				Genode::memcpy(buf_ptr, &req, sizeof(req));;
				return true;
			}
			case Request::REMOVE_KEY:
			{
				Key key;
				key.id.value = channel._request._key_id;
				Cbe::Request cbe_req { Cbe::Request::Operation::READ, false, 0, 0, 1, 0, idx };
				Crypto_request req {
					CRYPTA, idx, Crypto_request::REMOVE_KEY, cbe_req, key,
					(addr_t)&channel._plain_data_blk,
					(addr_t)&channel._cipher_data_blk };

				if (sizeof(req) > buf_size) {
					class Bad_size_2 { };
					throw Bad_size_2 { };
				}
				Genode::memcpy(buf_ptr, &req, sizeof(req));;
				return true;
			}
			case Request::DECRYPT_BLOCK:
			{
				Key key;
				key.id.value = channel._request._key_id;
				Cbe::Request cbe_req { Cbe::Request::Operation::READ, false, channel._request._blk_nr, 0, 1, key.id.value, 0 };
				Crypto_request req {
					CRYPTA, idx, Crypto_request::DECRYPT_BLOCK,
					cbe_req, key, (addr_t)&channel._plain_data_blk,
					(addr_t)&channel._cipher_data_blk };

				if (sizeof(req) > buf_size) {
					class Bad_size_2 { };
					throw Bad_size_2 { };
				}
				Genode::memcpy(buf_ptr, &req, sizeof(req));;
				return true;
			}
			case Request::ENCRYPT_BLOCK:
			{
				Key key;
				key.id.value = channel._request._key_id;
				Cbe::Request cbe_req { Cbe::Request::Operation::WRITE, false, channel._request._blk_nr, 0, 1, key.id.value, 0 };
				Crypto_request req {
					CRYPTA, idx, Crypto_request::ENCRYPT_BLOCK,
					cbe_req, key, (addr_t)&channel._plain_data_blk,
					(addr_t)&channel._cipher_data_blk };

				if (sizeof(req) > buf_size) {
					class Bad_size_2 { };
					throw Bad_size_2 { };
				}
				Genode::memcpy(buf_ptr, &req, sizeof(req));;
				return true;
			}
			default:
				class Bad_type { };
				throw Bad_type { };
			}
		}
	}
	return false;
}


void Cbe::Crypta::_drop_generated_request(Module_request &mod_req)
{
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

		class Bad_state { };
		throw Bad_state { };
	}
	_channels[id]._state = Channel::IN_PROGRESS;
}
