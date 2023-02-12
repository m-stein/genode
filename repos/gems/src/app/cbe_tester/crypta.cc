
/* base includes */
#include <base/log.h>

/* local includes */
#include <crypta.h>

using namespace Genode;
using namespace Cbe;


void Cbe::Crypta_request::create(
	void   *buf_ptr,
	size_t  buf_size,
	size_t  req_type,
	void   *prim_ptr,
	size_t  prim_size,
	void   *key_id_ptr,
	size_t  key_id_size,
	void   *key_plaintext_ptr,
	size_t  key_plaintext_size)
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

		log("Crypta_request::create: add key");
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
