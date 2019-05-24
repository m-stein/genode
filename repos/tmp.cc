#include <stdint.h>



static inline __attribute__((always_inline)) uint8_t _inb(uint16_t port)
{
	uint8_t res;
	asm volatile ("inb %%dx, %0" :"=a"(res) :"Nd"(port));
	return res;
}


static inline __attribute__((always_inline)) void _outb(uint16_t port, uint8_t val)
{
	asm volatile ("outb %b0, %w1" : : "a" (val), "Nd" (port));
}


enum {

	COMPORT_DATA_OFFSET   = 0,
	COMPORT_STATUS_OFFSET = 5,

	STATUS_THR_EMPTY = 0x20,  /* transmitter hold register empty */
	STATUS_DHR_EMPTY = 0x40,  /* data hold register empty - data completely sent */

	baud_rate = 115200,
	port = 1016,
	IER  = port + 1,
	EIR  = port + 2,
	LCR  = port + 3,
	MCR  = port + 4,
	LSR  = port + 5,
	MSR  = port + 6,
	DLLO = port + 0,
	DLHI = port + 1
};


int main (void) {

	asm volatile("nop");
	_outb(LCR, 0x80);  /* select bank 1 */
	for (volatile int i = 10000000; i--; );
	_outb(DLLO, (115200/baud_rate) >> 0);
	_outb(DLHI, (115200/baud_rate) >> 8);
	_outb(LCR, 0x03);  /* set 8,N,1 */
	_outb(IER, 0x00);  /* disable interrupts */
	_outb(EIR, 0x07);  /* enable FIFOs */
	_outb(MCR, 0x0b);  /* force data terminal ready */
	_outb(IER, 0x01);  /* enable RX interrupts */
	_inb(IER);
	_inb(EIR);
	_inb(LCR);
	_inb(MCR);
	_inb(LSR);
	_inb(MSR);

	/* wait until serial port is ready */
	uint8_t ready = STATUS_THR_EMPTY;
	while ((_inb(port + COMPORT_STATUS_OFFSET) & ready) != ready);

	/* output character */
	_outb(port + COMPORT_DATA_OFFSET, 'A');
	asm volatile("nop");

	return 0;
}
