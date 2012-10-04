
/* Genode includes */
#include <base/printf.h>

/* verilator includes */
#include "Vtop.h"
#include "verilated.h"

using namespace Genode;


void _bus_cycle(Vtop * const top)
{
	assert(!Verilated::gotFinish());
	static unsigned cycles = 0;
	cycles++;
	Genode::printf("----- clk rising edge %u -----\n", cycles);
	top->clk_i = 0;
	top->eval();
	top->clk_i = 1;
	top->eval();
	Genode::printf("ACK %x\n", top->ack_o);
	Genode::printf("DAT %x\n", top->dat_o);
}


/**
 * do reset operation at bus interface
 */
void _bus_reset(Vtop * const top)
{
	top->rst_i = 0;
	top->eval();
	top->rst_i = 1;
	top->cyc_i = 0;
	top->stb_i = 0;
	top->eval();
	top->rst_i = 0;
}

//struct Has_trait_tpl
//{
//	typedef char Yes;
//	typedef long No;
//
//	template <class C> static No test(...);
//};
//
//template <class T>
//struct Has_sel_i : public Has_trait_tpl
//{
//	template <class C> static Yes test(typeof(&C::sel_i));
//
//	enum { VALUE = sizeof(test<T>(0)) == sizeof(Yes) };
//};
//
//template <class T>
//struct Has_err_o : public Has_trait_tpl
//{
//	template <class C> static Yes test(typeof(&C::err_o));
//
//	enum { VALUE = sizeof(test<T>(0)) == sizeof(Yes) };
//};
//
//template <class T>
//struct Has_err_o : public Has_trait_tpl
//{
//	template <class C> static Yes test(typeof(&C::err_o));
//
//	enum { VALUE = sizeof(test<T>(0)) == sizeof(Yes) };
//};
//
//enum {
//	VTOP_HAS_SEL_I = Has_sel_i<Vtop>::VALUE,
//	VTOP_HAS_ERR_O = Has_err_o<Vtop>::VALUE,
//	VTOP_HAS_RTY_O = Has_rty_o<Vtop>::VALUE,
//};


int main(int argc, char **argv, char **env)
{
	Genode::printf("--- test verilator runtime enviroment ---\n");

	Verilated::commandArgs(argc, argv);
	Vtop * const top = new Vtop;

	_bus_reset(top);
	enum { MAX_SLAVE_CYCLES = 1000 };
	unsigned slave_cycles = 0;

	/* apply transfer parameters to the bus */
	top->dat_i = (uint32_t)0x12345678;
	top->adr_i = (uint32_t)0x0;
	top->we_i = 1;

	/* do a transfer-start cycle at the bus */
	top->stb_i = 1;
	top->cyc_i = 1;
	_bus_cycle(top);

	/* wait for feedback from the slave  */
	while (1) {
		if (top->ack_o)
		{
			/* do a transfer-end cycle */
			top->cyc_i = 0;
			top->stb_i = 0;
			_bus_cycle(top);
			break;
		}
		/* check conditions to continue waiting for feedback */
		assert(!top->err_o && !top->rty_o);
		assert(slave_cycles < MAX_SLAVE_CYCLES);
		slave_cycles++;
	}

	/* apply transfer parameters to the bus */
	top->adr_i = (uint32_t)0x0;
	top->we_i = 0;

	/* do a transfer-start cycle at the bus */
	top->stb_i = 1;
	top->cyc_i = 1;
	_bus_cycle(top);

	/* wait for feedback from the slave  */
	while (1) {
		if (top->ack_o)
		{
			/* do a transfer-end cycle */
			Genode::printf("READ %x\n", top->dat_o);
			top->cyc_i = 0;
			top->stb_i = 0;
			_bus_cycle(top);
			break;
		}
		/* check conditions to continue waiting for feedback */
		assert(!top->err_o && !top->rty_o);
		assert(slave_cycles < MAX_SLAVE_CYCLES);
		slave_cycles++;
	}
}
