#include <drivers/uart/pl011_base.h>
#include <drivers/board/vea9x4.h>
#include <drivers/cpu/cortex_a9/core.h>
#include <drivers/pic/pl390_base.h>


namespace Genode {

	 /* UART 3 */
	class Console : public Pl011_base
	{
		public:

			enum { IRQ = 40 };

			Console()
			: Pl011_base(Vea9x4::SMB_CS7_BASE + 0xc000,
			             Vea9x4::PL011_0_CLOCK, 38400) { }

			void printk(const char *s)
			{
				for (unsigned i = 0; s[i] != 0; i++)
					put_char(s[i]);
			}
	};


	class Pic : public Pl390_base
	{
		public:

			Pic()
			: Pl390_base(Cortex_a9::PL390_DISTRIBUTOR_MMIO_BASE,
			             Cortex_a9::PL390_CPU_MMIO_BASE) { }
	};
}


extern void* _exception_vector;

static Genode::Console *console;
static Genode::Pic     *pic;


extern "C" void exception_entry() {
	console->printk("--------> exception <---------\n"); }


static inline void set_vector_base(Genode::addr_t addr) {
	asm volatile ("mcr p15, 0, %0, c12, c0, 0" : : "r" (addr)); }

extern "C" void _main() {
	Genode::Console _console;
	Genode::Pic     _pic;
	console = &_console;
	pic     = &_pic;

	console->printk("Kernel started!\n");
	set_vector_base((Genode::addr_t)&_exception_vector);
	console->enable_rx_irq();
	pic->unmask(Genode::Console::IRQ);

	/* copy sp to system-mode, change to system-mode and enable IRQs */
	asm volatile ("push {r0}      \n"
	              "mov r0, sp     \n"
	              "cps #31        \n"
	              "mov sp, r0     \n"
	              "pop {r0}       \n"
	              "cpsie aif      \n");

	console->printk("go into endless loop\n");
	while (true) ;
}
