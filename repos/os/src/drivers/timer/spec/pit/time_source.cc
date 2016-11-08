/*
 * \brief  Platform timer based on the Programmable Interval Timer (PIT)
 * \author Norman Feske
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <time_source.h>

using namespace Genode;
using Microseconds = Genode::Time_source::Microseconds;

enum {
	PIT_TICKS_PER_SECOND = 1193182,
	PIT_TICKS_PER_MSEC   = PIT_TICKS_PER_SECOND/1000,
	PIT_MAX_COUNT        =   65535,
	PIT_DATA_PORT_0      =    0x40,  /* data port for PIT channel 0,
                                        connected to the PIC */
	PIT_CMD_PORT         =    0x43,  /* PIT command port */

	PIT_MAX_USEC = (PIT_MAX_COUNT*1000)/(PIT_TICKS_PER_MSEC),

	IRQ_PIT = 0,  /* timer interrupt at the PIC */

	/*
	 * Bit definitions for accessing the PIT command port
	 */
	PIT_CMD_SELECT_CHANNEL_0 = 0 << 6,
	PIT_CMD_ACCESS_LO        = 1 << 4,
	PIT_CMD_ACCESS_LO_HI     = 3 << 4,
	PIT_CMD_MODE_IRQ         = 0 << 1,
	PIT_CMD_MODE_RATE        = 2 << 1,

	PIT_CMD_READ_BACK        = 3 << 6,
	PIT_CMD_RB_COUNT         = 0 << 5,
	PIT_CMD_RB_STATUS        = 0 << 4,
	PIT_CMD_RB_CHANNEL_0     = 1 << 1,

	/*
	 * Bit definitions of the PIT status byte
	 */
	PIT_STAT_INT_LINE = 1 << 7,
};


void Timer::Time_source::_set_counter(uint16_t value)
{
	_handled_wrap = false;
	_io_port.outb(PIT_DATA_PORT_0, value & 0xff);
	_io_port.outb(PIT_DATA_PORT_0, (value >> 8) & 0xff);
}


uint16_t Timer::Time_source::_read_counter(bool *wrapped)
{
	/* read-back count and status of counter 0 */
	_io_port.outb(PIT_CMD_PORT, PIT_CMD_READ_BACK |
	                            PIT_CMD_RB_COUNT |
	                            PIT_CMD_RB_STATUS |
	                            PIT_CMD_RB_CHANNEL_0);

	/* read status byte from latch register */
	uint8_t status = _io_port.inb(PIT_DATA_PORT_0);

	/* read low and high bytes from latch register */
	uint16_t lo = _io_port.inb(PIT_DATA_PORT_0);
	uint16_t hi = _io_port.inb(PIT_DATA_PORT_0);

	*wrapped = status & PIT_STAT_INT_LINE ? true : false;
	return (hi << 8) | lo;
}


Microseconds Timer::Time_source::max_timeout() const { return PIT_MAX_USEC; }


void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	_handler = &handler;
	_timer_irq.ack_irq();

	/* limit timer-interrupt rate */
	enum { MAX_TIMER_IRQS_PER_SECOND = 4*1000 };
	if (duration < 1000*1000/MAX_TIMER_IRQS_PER_SECOND)
		duration = 1000*1000/MAX_TIMER_IRQS_PER_SECOND;

	if (duration > max_timeout())
		duration = max_timeout();

	_counter_init_value = (PIT_TICKS_PER_MSEC * duration)/1000;
	_set_counter(_counter_init_value);
}


Microseconds Timer::Time_source::curr_time() const
{
	uint32_t passed_ticks;

	/*
	 * Read PIT count and status
	 *
	 * Reading the PIT registers via port I/O is a non-const operation.
	 * Since 'curr_time' is declared as const, however, we need to
	 * explicitly override the const-ness of the 'this' pointer.
	 */
	bool wrapped;
	uint16_t const curr_counter =
		const_cast<Time_source *>(this)->_read_counter(&wrapped);

	/* determine the time since we looked at the counter */
	if (wrapped && !_handled_wrap) {
		passed_ticks = _counter_init_value;
		/* the counter really wrapped around */
		if (curr_counter)
			passed_ticks += PIT_MAX_COUNT + 1 - curr_counter;

		_handled_wrap = true;
	} else {
		if (_counter_init_value)
			passed_ticks = _counter_init_value - curr_counter;
		else
			passed_ticks = PIT_MAX_COUNT + 1 - curr_counter;
	}

	_curr_time += (passed_ticks*1000)/PIT_TICKS_PER_MSEC;

	/* use current counter as the reference for the next update */
	_counter_init_value = curr_counter;

	return _curr_time;
}


Timer::Time_source::Time_source(Entrypoint &ep)
:
	Signalled_time_source(ep),
	_io_port(PIT_DATA_PORT_0, PIT_CMD_PORT - PIT_DATA_PORT_0 + 1),
	_timer_irq(IRQ_PIT), _curr_time(0), _counter_init_value(0),
	_handled_wrap(false)
{
	/* operate PIT in one-shot mode */
	_io_port.outb(PIT_CMD_PORT, PIT_CMD_SELECT_CHANNEL_0 |
	              PIT_CMD_ACCESS_LO_HI | PIT_CMD_MODE_IRQ);

	_timer_irq.sigh(_signal_handler);
}
