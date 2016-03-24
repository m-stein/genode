/*
 * \brief  Test for timer service
 * \author Norman Feske
 * \date   2009-06-22
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <kernel/interface.h>
#include <base/signal.h>

using namespace Genode;

int main(int argc, char **argv)
{
	printf("--- kernel timer test ---\n");

	Signal_receiver sigrec;
	Signal_context  sigctx;
	Kernel::capid_t const sigid = sigrec.manage(&sigctx).dst();
	for (unsigned i = 0; ; i++) {
		Kernel::timeout(333000, sigid);
		sigrec.wait_for_signal();
		PLOG("%u", i);
	}

	return 0;
}
