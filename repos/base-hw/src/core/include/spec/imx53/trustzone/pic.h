/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

/* core includes */
#include <spec/imx53/pic_support.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Imx53_pic
{
	public:

		/**
		 * Constructor
		 */
		Pic()
		{
			for (unsigned i = 0; i < NR_OF_IRQ; i++) { secure(i); }
			write<Priomask::Mask>(0xff);
		}

		/**
		 * Make an interrupt available for the unsecure world
		 *
		 * \param i  targeted interrupt
		 */
		void unsecure(unsigned const i)
		{
			if (i < NR_OF_IRQ) {
				write<Intsec::Nonsecure>(1, i);
				write<Priority>(0x80, i);
			}
		}

		/**
		 * Make an interrupt unavailable for the unsecure world
		 *
		 * \param i  targeted interrupt
		 */
		void secure(unsigned const i)
		{
			if (i < NR_OF_IRQ) {
				write<Intsec::Nonsecure>(0, i);
				write<Priority>(0, i);
			}
		}
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
