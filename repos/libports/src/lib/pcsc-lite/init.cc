/*
 * \brief  pcsc-lite initialization
 * \author Christian Prochaska
 * \date   2016-10-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/xml_node.h>

/* libc includes */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* pcsc-lite includes */
extern "C" {
#include <debuglog.h>
#include <readerfactory.h>
}

static constexpr bool verbose = false;

struct Pcsc_lite_initializer
{
	Pcsc_lite_initializer()
	{
		if (verbose) {
			/* pcscd -f */
			DebugLogSetLogType(DEBUGLOG_STDOUT_DEBUG);
			/* pcscd -d */
			DebugLogSetLevel(PCSC_LOG_DEBUG);
			/* pcscd -a */
			(void)DebugLogSetCategory(DEBUG_CATEGORY_APDU);
		}

		/*
		 * Find out vendor id and product id of the connected USB device
		 * and add it as reader.
		 */

		char device[] { 0 };

		RFAllocateReaderSpace(0);
		(void)RFAddReader("Secure Flash Card", 0, "/", device);
	}
};


extern "C" void initialize_pcsc_lite()
{
	static Pcsc_lite_initializer pcsc_lite_initializer;
}
