
#include <cbe/module.h>
#include <cbe/types.h>

using namespace Genode;

Cbe::Module_request::Module_request(unsigned long src_module_id,
                                    unsigned long src_request_id,
                                    unsigned long dst_module_id)
:
	_src_module_id  { src_module_id },
	_src_request_id { src_request_id },
	_dst_module_id  { dst_module_id }
{
	log(
		"Module_request::", __func__, " src ", _src_module_id, " src req ",
		_src_request_id, " dst ", _dst_module_id);
}
