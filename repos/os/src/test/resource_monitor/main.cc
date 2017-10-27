/*
 * \brief  Diversified test of the Register and MMIO framework
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>

using namespace Genode;
using namespace Timer;

struct Allocation : List<Allocation>::Element, Noncopyable
{
	static unsigned long      count;
	Ram_session              &ram;
	Ram_dataspace_capability  ds { ram.alloc(1024) };

	Allocation(Ram_session &ram) : ram(ram) { count++; }

	~Allocation()
	{
		ram.free(ds);
		count--;
	}
};

unsigned long Allocation::count { 0 };

struct Main
{
	using Start_direction = String<4>;

	Env                    &env;
	Attached_rom_dataspace  config        { env, "config" };
	Timer::Connection       timer         { env };
	Periodic_timeout<Main>  timeout       { timer, *this, &Main::handle_timeout,
	                                        Microseconds(config.xml().attribute_value("period_ms", 1000UL) * 1000UL) };
	Heap                    heap          { env.ram(), env.rm() };
	bool          const     verbose       { config.xml().attribute_value("verbose", false) };
	size_t        const     total_kib     { config.xml().attribute_value("total_kib", 1024UL) };
	unsigned long const     steps_up      { config.xml().attribute_value("steps_up", 8UL) };
	unsigned long const     steps_down    { config.xml().attribute_value("steps_down", 8UL) };
	size_t        const     step_kib_up   { total_kib / steps_up };
	size_t        const     step_kib_down { total_kib / steps_down };
	bool                    up            { config.xml().attribute_value("start_direction", Start_direction("up"))
	                                        == Start_direction("up") };
	unsigned long           step          { 0 };
	unsigned long           step_total    { 0 };
	List<Allocation>        allocs;

	Main(Env &env) : env(env)
	{
		if (!up) { allocate(total_kib); }
	}

	void allocate(unsigned long kib)
	{
		for (unsigned long i = 0; i < kib; i++) {
			allocs.insert(new (heap) Allocation(env.ram()));
		}
	}

	void free(unsigned long kib)
	{
		for (unsigned long i = 0; i < step_kib_down; i++) {
			Allocation *alloc = allocs.first();
			allocs.remove(alloc);
			destroy(&heap, alloc);
		}
	}

	void change_direction()
	{
		up = !up;
		step = 0;
		update_allocs();
	}

	void handle_timeout(Duration)
	{
		update_allocs();
		step_total++;
		step++;
	}

	void update_allocs()
	{
		if (up) {
			if (step < steps_up) {
				allocate(step_kib_up);
				log_step(step_kib_up);
			} else {
				change_direction();
			}
		} else {
			if (step < steps_down) {
				free(step_kib_down);
				log_step(step_kib_down);
			} else {
				change_direction();
			}
		}
	}

	void log_step(unsigned long kib)
	{
		if (!verbose) { return; }
		log("step ", step_total, ": ",
		    up ? "allocated" : "freed", " ", kib, " kib, ",
		    "total ", Allocation::count, " kib");
	}
};

void Component::construct(Genode::Env &env)
{
	static Main main(env);
}

