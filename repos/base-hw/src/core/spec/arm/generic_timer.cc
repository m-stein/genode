/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <drivers/timer/util.h>
#include <kernel/timer.h>
#include <kernel/cpu.h>

using namespace Kernel;


unsigned Timer::interrupt_id() const { return Board::TIMER_IRQ; }


unsigned long Board::Timer::_freq() { return Genode::Cpu::Cntfrq::read(); }


Board::Timer::Timer(unsigned) : ticks_per_ms(_freq() / 1000)
{
	Genode::Cpu::Cntp_ctl::access_t ctl = 0;
	Genode::Cpu::Cntp_ctl::Enable::set(ctl, 1);
	Genode::Cpu::Cntp_ctl::write(ctl);
}


void Timer::_start_one_shot(time_t const ticks)
{
	_device.last_time = Genode::Cpu::Cntpct::read();
	Genode::Cpu::Cntp_tval::write(ticks);
	Genode::Cpu::Cntp_ctl::access_t ctl = Genode::Cpu::Cntp_ctl::read();
	Genode::Cpu::Cntp_ctl::Istatus::set(ctl, 0);
	Genode::Cpu::Cntp_ctl::write(ctl);
}


time_t Timer::_duration() const
{
	return Genode::Cpu::Cntpct::read() - _device.last_time;
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return Genode::timer_ticks_to_us(ticks, _device.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * _device.ticks_per_ms; }


time_t Timer::_max_value() const {
	return _device.ticks_per_ms * 5000; }
