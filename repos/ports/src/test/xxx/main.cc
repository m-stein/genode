/*
 * \brief  GDB Monitor test
 * \author Christian Prochaska
 * \date   2011-05-24
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

void mread(int fd, char *buf, size_t buf_size, off_t off)
{
	signed long total = 0;
	while (1) {
		signed long bytes = pread(fd, buf + total,
			buf_size - total, off + total);

		printf ("Bla %zd\n", bytes);
		if (bytes < 0) {
			throw -1;
		}
		total += bytes;
		if (total >= buf_size) {
			break;
		}
	}
}

int main (int argc, char *argv[])
{
    int fd1;
    char buf[16384i];
    fd1 = open("/dev/cbe/current/data", O_RDWR);
    if (fd1 == -1) {
        printf("Failed to open file\n");
        return 1;
    }
    mread (fd1, buf, sizeof(buf), 512);
    close(fd1);
    printf("Test successful\n");
    return 0;
}
