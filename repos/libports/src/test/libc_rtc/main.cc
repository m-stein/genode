/*
 * \brief  pcsc-lite test
 * \author Christian Prochaska
 * \date   2016-09-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>

#include <unistd.h>
#include <time.h>

int main()
{
	unsigned idx = 1;
	while (1)
	{
		struct timespec ts;
		if (clock_gettime(0, &ts)) {
			return -1;
		}

		struct tm *tm = localtime((time_t*)&ts.tv_sec);
		if (!tm) {
			return -1;
		}

		printf("Timestamp #%d: %d-%d-%d %d:%d %ds\n",
			idx++,
			1900 + tm->tm_year,
			1 + tm->tm_mon,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec);

		sleep(1);
	}

	return 0;
}
