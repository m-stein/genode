
#ifndef _PORT_ALLOCATOR_H_
#define _PORT_ALLOCATOR_H_

#include <util/bit_allocator.h>
#include <base/lock.h>

namespace Net {

	enum { NR_OF_PORTS = ((Genode::uint16_t)~0) + 1 };

	class Port_allocator;
}

class Net::Port_allocator : public Genode::Bit_allocator<NR_OF_PORTS>
{
	private:

		Genode::Lock _lock;

	public:

		Port_allocator();

		Genode::addr_t alloc(Genode::size_t const num_log2 = 0);

		void free(
			Genode::addr_t const bit_start, Genode::size_t const num_log2 = 0);
};

#endif /* _PORT_ALLOCATOR_H_ */
