
#include <cbe/module.h>

Cbe::Module_request::Module_request(unsigned long dst_module_id)
:
	_valid         { true },
	_dst_module_id { dst_module_id }
{ }
