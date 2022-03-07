

#include <lx_emul/irq.h>
#include <lx_kit/env.h>

extern "C" unsigned int lx_emul_irq_last()
{
	return Lx_kit::env().last_irq;
}

