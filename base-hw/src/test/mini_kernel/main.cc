#include <drivers/uart/pl011_base.h>
#include <drivers/board/vea9x4.h>
#include <drivers/cpu/cortex_a9/core.h>

namespace Genode {

	 /* UART 0 */
	class Console : public Pl011_base
	{
		public:

			enum { IRQ = 37 };

			Console()
			: Pl011_base(Vea9x4::SMB_CS7_BASE + 0x9000,
			             Vea9x4::PL011_0_CLOCK, 115200) { enable_rx_irq(); }

			void printk(const char *s)
			{
				for (unsigned i = 0; s[i] != 0; i++)
					put_char(s[i]);
			}
	};


	class Pic
	{
		public:

			enum { MAX_INTERRUPT_ID = 1023 };

		protected:

			enum {
				MIN_SPI  = 32,
			};

			/**
			 * Distributor interface
			 */
			struct Distr : public Mmio
			{
				Distr(addr_t const base) : Mmio(base) { }

				/**
				 * Distributor control register
				 */
				struct Icddcr : Register<0x000, 32>
				{
					struct Enable : Bitfield<0,1> { };
				};

				/**
				 * Interrupt controller type register
				 */
				struct Icdictr : Register<0x004, 32>
				{
					struct It_lines_number : Bitfield<0,5>  { };
					struct Cpu_number      : Bitfield<5,3>  { };
				};

				/**
				 * Interrupt set enable registers
				 */
				struct Icdiser : Register_array<0x100, 32, MAX_INTERRUPT_ID+1, 1, true>
				{
					struct Set_enable : Bitfield<0, 1> { };
				};

				/**
				 * Interrupt clear enable registers
				 */
				struct Icdicer : Register_array<0x180, 32, MAX_INTERRUPT_ID+1, 1, true>
				{
					struct Clear_enable : Bitfield<0, 1> { };
				};

				/**
				 * Interrupt priority level registers
				 */
				struct Icdipr  : Register_array<0x400, 32, MAX_INTERRUPT_ID+1, 8>
				{
					struct Priority : Bitfield<0, 8>
					{
						enum { GET_MIN_PRIORITY = 0xff };
					};
				};

				/**
				 * Interrupt processor target registers
				 */
				struct Icdiptr : Register_array<0x800, 32, MAX_INTERRUPT_ID+1, 8>
				{
					struct Cpu_targets : Bitfield<0, 8>
					{
						enum { ALL = 0xff };
					};
				};

				/**
				 * Interrupt configuration registers
				 */
				struct Icdicr : Register_array<0xc00, 32, MAX_INTERRUPT_ID+1, 2>
				{
					struct Edge_triggered : Bitfield<1, 1> { };
				};

				/**
				 * ID of the maximum supported interrupt
				 */
				Icdictr::access_t max_interrupt()
				{
					enum { LINE_WIDTH_LOG2 = 5 };
					Icdictr::access_t lnr = read<Icdictr::It_lines_number>();
					return ((lnr + 1) << LINE_WIDTH_LOG2) - 1;
				}

			} _distr;

			/**
			 * CPU interface
			 */
			struct Cpu : public Mmio
			{
				Cpu(addr_t const base) : Mmio(base) { }

				/**
				 * CPU interface control register
				 */
				struct Iccicr : Register<0x00, 32>
				{
					struct Enable : Bitfield<0,1> { };
				};

				/**
				 * Priority mask register
				 */
				struct Iccpmr : Register<0x04, 32>
				{
					struct Priority : Bitfield<0,8> { };
				};

				/**
				 * Binary point register
				 */
				struct Iccbpr : Register<0x08, 32>
				{
					struct Binary_point : Bitfield<0,3>
					{
						enum { NO_PREEMPTION = 7 };
					};
				};
			} _cpu;

			unsigned const _max_interrupt;

		public:

			/**
			 * Constructor, all interrupts get masked
			 */
			Pic(bool init = true)
			: _distr(Cortex_a9::PL390_DISTRIBUTOR_MMIO_BASE),
			  _cpu(Cortex_a9::PL390_CPU_MMIO_BASE),
			  _max_interrupt(_distr.max_interrupt())
			{
				/* configure every shared peripheral interrupt */
				for (unsigned i=MIN_SPI; i <= _max_interrupt; i++)
				{
					_distr.write<Distr::Icdicr::Edge_triggered>(0, i);
					_distr.write<Distr::Icdipr::Priority>(0, i);
					_distr.write<Distr::Icdiptr::Cpu_targets>(Distr::Icdiptr::Cpu_targets::ALL, i);

					/* enable all irqs */
					_distr.write<Distr::Icdiser::Set_enable>(1, i);
				}

				/* disable the priority filter */
				_cpu.write<Cpu::Iccpmr::Priority>(0xff);

				/* enable device */
				_cpu.write<Cpu::Iccicr::Enable>(1);
				_cpu.write<Cpu::Iccbpr::Binary_point>(7);
				_distr.write<Distr::Icddcr::Enable>(1);
			}
	};
}


extern void* _exception_vector;

static Genode::Console *console;
static Genode::Pic     *pic;


extern "C" void exception_entry() {
	volatile int exc = 0;
	asm volatile ("mov %0, r0\n" : "=r" (exc));

	switch(exc) {
	case 1:
		console->printk("Reset\n");
		break;
	case 2:
		console->printk("Undef\n");
		break;
	case 3:
		console->printk("Smc\n");
		break;
	case 4:
		console->printk("Prefetch abort\n");
		break;
	case 5:
		console->printk("Data Abort\n");
		break;
	case 6:
		console->printk("IRQ\n");
		break;
	case 7:
		console->printk("FIQ\n");
		break;
	default:
		console->printk("unknown exception\n");
	}
}


static inline void set_vector_base(Genode::addr_t addr) {
	asm volatile ("mcr p15, 0, %0, c12, c0, 0" : : "r" (addr)); }

extern "C" void user_loop() {
	while (true) ;
}

extern "C" void _main() {
	set_vector_base((Genode::addr_t)&_exception_vector);

	Genode::Console _console;
	Genode::Pic     _pic;
	console = &_console;
	pic     = &_pic;

	console->printk("Kernel started!\n");

	/* change to user-mode and enable IRQs */
	asm volatile ("mov  r0, #16       \n"
	              "msr  spsr, r0      \n"
	              "mov  r0, %[instr]  \n"
	              "push {r0}          \n"
	              "push {sp}          \n"
	              "ldm  sp, {sp, pc}^ \n"
	              :: [instr] "r" (&user_loop));
	console->printk("exit kernel\n");
}
