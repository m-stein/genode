
#include <cbe/module.h>
#include <cbe/types.h>

using namespace Genode;

Cbe::Module_request::Module_request(unsigned long src_module_id,
                                    unsigned long dst_module_id)
:
	_src_module_id { src_module_id },
	_dst_module_id { dst_module_id }
{
	log(__func__, " src ", _src_module_id, " dst ", _dst_module_id);
}
