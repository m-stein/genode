
#include <port_allocator.h>
#include <base/log.h>

using namespace Net;
using namespace Genode;

Port_allocator::Port_allocator()
{
	if (alloc() == 0) { return; }
	error("Can't preserve port 0");
	class Cant_preserve_port_0 : public Exception { };
	throw Cant_preserve_port_0();
}


addr_t Port_allocator::alloc(size_t const num_log2)
{
	Lock::Guard lock_guard(_lock);
	return Bit_allocator::alloc(num_log2);
}


void Port_allocator::free(addr_t const bit_start, size_t const num_log2)
{
	Lock::Guard lock_guard(_lock);
	Bit_allocator::free(bit_start, num_log2);
}
