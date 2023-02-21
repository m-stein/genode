/* local includes */
#include <crypto.h>
#include <cbe_librara.h>


void Cbe::Librara::_drop_generated_request(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Bad_call { };
		throw Bad_call { };
	}
	if (mod_req.dst_module_id() != CRYPTO) {
		class Bad_call { };
		throw Bad_call { };
	}
	_lib->librara__drop_generated_request(
		dynamic_cast<Crypto_request *>(&mod_req)->prim());
}


void Cbe::Librara::generated_request_complete(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Bad_call { };
		throw Bad_call { };
	}
	if (mod_req.dst_module_id() != CRYPTO) {
		class Bad_call { };
		throw Bad_call { };
	}
	Crypto_request &req { *dynamic_cast<Crypto_request *>(&mod_req) };
	_lib->librara__generated_request_complete(
		req.prim(), req.result_blk_ptr(), req.success());
}
