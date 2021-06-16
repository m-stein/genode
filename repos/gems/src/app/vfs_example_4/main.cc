/*
 * \brief  Expat test
 * \author Christian Prochaska
 * \date   2012-06-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	static char buf[128];
	int config_fd = open("/friendly/greetings", O_RDONLY);
	if (config_fd < 0) {
		printf("Error: could not open file\n");
		return -1;
	}
	read(config_fd, buf, sizeof(buf) - 1);
	printf("%s\n", buf);
	return 0;
}
