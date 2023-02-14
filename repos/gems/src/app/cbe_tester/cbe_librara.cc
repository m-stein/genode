/* local includes */
#include <crypta.h>
#include <cbe_librara.h>


void Cbe::Librara::_drop_generated_request(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Bad_call { };
		throw Bad_call { };
	}
	if (mod_req.dst_module_id() != CRYPTA) {
		class Bad_call { };
		throw Bad_call { };
	}
	Crypta_request &crypta_req { *dynamic_cast<Crypta_request *>(&mod_req) };
	void *data_blk_ptr { nullptr };
	switch (crypta_req.type()) {
	case Crypta_request::DECRYPT_BLOCK:
		data_blk_ptr = crypta_req.cipher_data_blk_ptr();
		break;
	case Crypta_request::ENCRYPT_BLOCK:
		data_blk_ptr = crypta_req.plain_data_blk_ptr();
		break;
	default:
		break;
	}
	_lib->librara__drop_generated_request(crypta_req.prim(), data_blk_ptr);
}


void Cbe::Librara::generated_request_complete(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Bad_call { };
		throw Bad_call { };
	}
	if (mod_req.dst_module_id() != CRYPTA) {
		class Bad_call { };
		throw Bad_call { };
	}
	Crypta_request &crypta_req { *dynamic_cast<Crypta_request *>(&mod_req) };
	void *data_blk_ptr { nullptr };
	switch (crypta_req.type()) {
	case Crypta_request::DECRYPT_BLOCK:
		data_blk_ptr = crypta_req.plain_data_blk_ptr();
		break;
	case Crypta_request::ENCRYPT_BLOCK:
		data_blk_ptr = crypta_req.cipher_data_blk_ptr();
		break;
	default:
		break;
	}
	_lib->librara__generated_request_complete(
		crypta_req.prim(), data_blk_ptr, crypta_req.success());
}
