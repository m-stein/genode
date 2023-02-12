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
	_lib->librara__drop_generated_request(dynamic_cast<Crypta_request *>(&mod_req)->prim());
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
	_lib->librara__generated_request_complete(
		dynamic_cast<Crypta_request *>(&mod_req)->prim(),
		mod_req.success());
}
