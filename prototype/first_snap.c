/*
 * Copyright 2017 Adam H. Leventhal. All Rights Reserved.
 */

#include <fcntl.h>
#include <unistd.h>

#include <sys/syscall.h>

int
main(int argc, char **argv)
{
	int ret;
	int dirfd = open(argv[1], O_RDONLY, 0);
	if (dirfd < 0) {
		perror("open");
		exit(1);
	}
        ret = syscall(SYS_fs_snapshot, 0x01, dirfd, argv[2], NULL, NULL, 0);
	if (ret != 0)
		perror("fs_snapshot");

	return (0);
}
