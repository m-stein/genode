/**
 * \brief  AHCI port controller handling
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <timer_session/connection.h>

/* local includes */
#include <ata_driver.h>
#include <atapi_driver.h>


struct Ahci
{
	Genode::Env       &env;
	Genode::Allocator &alloc;

	/* read device signature */
	enum Signature {
		ATA_SIG        = 0x101,
		ATAPI_SIG      = 0xeb140101,
		ATAPI_SIG_QEMU = 0xeb140000, /* will be fixed in Qemu */
	};

	struct Timer_delayer : Mmio::Delayer, Timer::Connection
	{
		Timer_delayer(Genode::Env &env)
		: Timer::Connection(env) { }

		void usleep(unsigned us) { Timer::Connection::usleep(us); }
	} _delayer { env };

	Ahci_root     &root;
	Platform::Hba &platform_hba = Platform::init(env, _delayer);
	Hba            hba          { env, platform_hba, _delayer };

	enum { MAX_PORTS = 32 };
	Port_driver   *ports[MAX_PORTS];
	bool           port_claimed[MAX_PORTS];

	Signal_handler<Ahci> irq;
	unsigned             ready_count = 0;
	bool                 enable_atapi;

	Ahci(Genode::Env &env, Genode::Allocator &alloc,
	     Ahci_root &root, bool support_atapi)
	:
		env(env), alloc(alloc),
		root(root), irq(root.entrypoint(), *this, &Ahci::handle_irq),
		enable_atapi(support_atapi)
	{
		info();

		/* register irq handler */
		platform_hba.sigh_irq(irq);

		/* initialize HBA (IRQs, memory) */
		hba.init();

		/* search for devices */
		scan_ports(env.rm(), env.ram());
	}

	bool atapi(unsigned sig)
	{
		return enable_atapi && (sig == ATAPI_SIG_QEMU || sig == ATAPI_SIG);
	}

	bool ata(unsigned sig)
	{
		return sig == ATA_SIG;
	}

	/**
	 * Forward IRQs to ports
	 */
	void handle_irq()
	{
		unsigned port_list = hba.read<Hba::Is>();
		while (port_list) {
			unsigned port = log2(port_list);
			port_list    &= ~(1U << port);

			ports[port]->handle_irq();
		}

		/* clear status register */
		hba.ack_irq();

		/* ack at interrupt controller */
		platform_hba.ack_irq();
	}

	void info()
	{
		using Genode::log;

		log("version: "
		    "major=", Genode::Hex(hba.read<Hba::Version::Major>()), " "
		    "minor=", Genode::Hex(hba.read<Hba::Version::Minor>()));
		log("command slots: ", hba.command_slots());
		log("native command queuing: ", hba.ncq() ? "yes" : "no");
		log("64-bit support: ", hba.supports_64bit() ? "yes" : "no");
	}

	void scan_ports(Genode::Region_map &rm, Genode::Ram_session &ram)
	{
		log("number of ports: ", hba.port_count(), " pi: ",
		    Hex(hba.read<Hba::Pi>()));

		unsigned available = hba.read<Hba::Pi>();
		for (unsigned i = 0; i < hba.port_count(); i++) {

			/* check if port is implemented */
			if (!(available & (1U << i)))
				continue;

			switch (Port_base(i, hba).read<Port_base::Sig>()) {

				case ATA_SIG:
					try {
						ports[i] = new (&alloc)
							Ata_driver(alloc, ram, root, ready_count, rm, hba,
							           platform_hba, i);
						log("\t\t#", i, ": ATA");
					} catch (...) {
						error("failed to initialize ATA port ", i); }
					break;

				case ATAPI_SIG:
				case ATAPI_SIG_QEMU:
					try {
						ports[i] = new (&alloc)
							Atapi_driver(ram, root, ready_count, rm, hba,
							             platform_hba, i);
						log("\t\t#", i, ": ATAPI");
					} catch (...) {
						error("failed to initialize ATAPI port ", i); }
					break;

				default:
					warning("unsupported device signature at port ", i);
			}
		}
	};

	Block::Driver *claim_port(unsigned port_num)
	{
		if (!avail(port_num))
			throw -1;

		port_claimed[port_num] = true;
		return ports[port_num];
	}

	void free_port(unsigned port_num)
	{
		port_claimed[port_num] = false;
	}

	bool avail(unsigned port_num)
	{
		return port_num < MAX_PORTS && ports[port_num] && !port_claimed[port_num] &&
		       ports[port_num]->ready();
	}

	long device_number(const char *model_num, const char *serial_num)
	{
		for (long port_num = 0; port_num < MAX_PORTS; port_num++) {
			Ata_driver* drv = dynamic_cast<Ata_driver *>(ports[port_num]);
			if (!drv)
				continue;

			if (*drv->model == model_num && *drv->serial == serial_num)
				return port_num;
		}
		return -1;
	}
};


static Ahci *sata_ahci(Ahci *ahci = 0)
{
	static Ahci *a = ahci;
	return a;
}


void Ahci_driver::init(Genode::Env &env, Genode::Allocator &alloc,
                       Ahci_root &root, bool support_atapi)
{
	static Ahci ahci(env, alloc, root, support_atapi);
	sata_ahci(&ahci);
}


Block::Driver *Ahci_driver::claim_port(long device_num)
{
	return sata_ahci()->claim_port(device_num);
}


void Ahci_driver::free_port(long device_num)
{
	sata_ahci()->free_port(device_num);
}


bool Ahci_driver::avail(long device_num)
{
	return sata_ahci()->avail(device_num);
}


long Ahci_driver::device_number(char const *model_num, char const *serial_num)
{
	return sata_ahci()->device_number(model_num, serial_num);
}
