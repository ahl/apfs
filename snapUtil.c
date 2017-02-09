/*
 * Copyright 2017 Adam H. Leventhal. All Rights Reserved.
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <unistd.h>
#include <errno.h>
#include <sys/attr.h>
#include <sys/stat.h>

#define	SNAPSHOT_OP_CREATE	0x01
#define	SNAPSHOT_OP_DELETE	0x02
#define	SNAPSHOT_OP_RENAME	0x03
#define	SNAPSHOT_OP_MOUNT	0x04
#define	SNAPSHOT_OP_REVERT	0x05

#define	FSOPT_LIST_SNAPSHOT	0x40

int
__fs_snapshot(int op, int dirfd, const char *name, const char *nw,
    void *mnt, uint32_t flags)
{
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	return (syscall(SYS_fs_snapshot, op, dirfd, name, nw, mnt, flags));
}

int
fs_snapshot_create(int dirfd, const char *name, uint32_t flags)
{
	return __fs_snapshot(SNAPSHOT_OP_CREATE, dirfd, name, NULL, NULL, flags);
}

int
fs_snapshot_list(int dirfd, struct attrlist *alist, void *attrbuf,
    size_t bufsize, uint32_t flags)
{
	if (flags != 0) {
		errno = EINVAL;
		return (-1);
	}

	return (getattrlistbulk(dirfd, alist, attrbuf, bufsize,
	    FSOPT_LIST_SNAPSHOT));
}

int
fs_snapshot_delete(int dirfd, const char *name, uint32_t flags)
{
	return __fs_snapshot(SNAPSHOT_OP_DELETE, dirfd, name, NULL, NULL, flags);
}

int
fs_snapshot_rename(int dirfd, const char *old, const char *nw, uint32_t flags)
{
	return __fs_snapshot(SNAPSHOT_OP_RENAME, dirfd, old, nw, NULL, flags);
}

int
fs_snapshot_revert(int dirfd, const char *name, uint32_t flags)
{
    return __fs_snapshot(SNAPSHOT_OP_REVERT, dirfd, name, NULL, NULL, flags);
}

#define FS_MOUNT_SNAPSHOT 2
#define MAX_SNAPSHOT_NAMELEN    256

struct fs_mount_options {
        uint32_t fs_flags;
        uint8_t _padding_[2];
};

struct fs_mount_args {
        char *specdev;
        struct fs_mount_options options;
        uint16_t mode;
        uint16_t _padding_[3];
        union {
                struct {                        // FS_MOUNT_SNAPSHOT
                        dev_t snap_fsys;
                        char snap_name[MAX_SNAPSHOT_NAMELEN];
                };
                struct {                        // APFS_MOUNT_FOR_CONVERSION
                };
        };
};

int
fs_snapshot_mount(int dirfd, const char *dir, const char *snapshot,
    uint32_t flags)
{
        struct stat st;
        struct fs_mount_args mnt_args;

        mnt_args.specdev = NULL;
        mnt_args.mode = FS_MOUNT_SNAPSHOT;
        if (fstat(dirfd, &st) == -1)
                return (-1);

        mnt_args.snap_fsys = st.st_dev;
        strlcpy(mnt_args.snap_name, snapshot, sizeof(mnt_args.snap_name));
        return (__fs_snapshot(SNAPSHOT_OP_MOUNT, dirfd, snapshot, dir,
            (void *)&mnt_args, flags));
}


const char *g_pname;

void
usage(void)
{
	(void) fprintf(stderr, "Usage:\n");
	(void) fprintf(stderr, "\t%s -l <vol>\n", g_pname);
	(void) fprintf(stderr, "\t%s -c <snap> <vol>\n", g_pname);
	(void) fprintf(stderr, "\t%s -n <snap> <newname> <vol>\n", g_pname);
	(void) fprintf(stderr, "\t%s -d <snap> <vol>\n", g_pname);
	(void) fprintf(stderr, "\t%s -r <snap> <vol>\n", g_pname);
	(void) fprintf(stderr, "\t%s -s <snap> <vol> <mntpnt>\n", g_pname);
	(void) fprintf(stderr, "\t%s -u <snap> <vol>\n", g_pname);
	exit(2);
}

int
do_create(const char *vol, const char *snap)
{
	int dirfd = open(vol, O_RDONLY, 0);
	if (dirfd < 0) {
		perror("open");
		exit(1);
	}

	int ret = fs_snapshot_create(dirfd, snap, 0);
	if (ret != 0)
		perror("fs_snapshot_create");
	return (ret);
}

int
do_delete(const char *vol, const char *snap)
{
	int dirfd = open(vol, O_RDONLY, 0);
	if (dirfd < 0) {
		perror("open");
		exit(1);
	}

	int ret = fs_snapshot_delete(dirfd, snap, 0);
	if (ret != 0)
		perror("fs_snapshot_delete");
	return (ret);
}

int
do_revert(const char *vol, const char *snap)
{
	int dirfd = open(vol, O_RDONLY, 0);
	if (dirfd < 0) {
		perror("open");
		exit(1);
	}

	int ret = fs_snapshot_revert(dirfd, snap, 0);
	if (ret != 0)
		perror("fs_snapshot_revert");
	return (ret);
}

int
do_rename(const char *vol, const char *snap, const char *nw)
{
	int dirfd = open(vol, O_RDONLY, 0);
	if (dirfd < 0) {
		perror("open");
		exit(1);
	}

	int ret = fs_snapshot_rename(dirfd, snap, nw, 0);
	if (ret != 0)
		perror("fs_snapshot_rename");
	return (ret);
}

int
do_mount(const char *vol, const char *snap, const char *mntpnt)
{
	int dirfd = open(vol, O_RDONLY, 0);
	if (dirfd < 0) {
		perror("open");
		exit(1);
	}

	int ret = fs_snapshot_mount(dirfd, mntpnt, snap, 0);
	if (ret != 0) {
		perror("fs_snapshot_mount");
	} else {
		printf("mount_apfs: snapshot implicitly mounted readonly\n");
	}
	return (ret);
}

int
do_list(const char *vol)
{
	int dirfd = open(vol, O_RDONLY, 0);
	if (dirfd < 0) {
		perror("open");
		exit(1);
	}

	struct attrlist alist = { 0 };
	char abuf[2048];

	alist.commonattr = ATTR_BULK_REQUIRED;

	int count = fs_snapshot_list(dirfd, &alist, &abuf[0], sizeof (abuf), 0);
	if (count < 0) {
		perror("fs_snapshot_list");
		exit(1);
	}

	char *p = &abuf[0];
	for (int i; i < count; i++) {
		char *field = p;
		uint32_t len = *(uint32_t *)field;
		field += sizeof (uint32_t);
		attribute_set_t attrs = *(attribute_set_t *)field;
		field += sizeof (attribute_set_t);

		if (attrs.commonattr & ATTR_CMN_NAME) {
			attrreference_t ar = *(attrreference_t *)field;
			char *name = field + ar.attr_dataoffset;
			field += sizeof (attrreference_t);
			(void) printf("%s\n", name);
		}

		p += len;
	}

	return (0);
}


int
main(int argc, char **argv)
{
	g_pname = strrchr(argv[0], '/') + 1;

	if (argc < 3 || argv[1][0] != '-' ||
	    argv[1][1] == '\0' || argv[1][2] != '\0') {
		usage();
	}

	switch (argv[1][1]) {
		case 'l':
			if (argc != 3)
				usage();
			return (do_list(argv[2]));
		case 'c':
			if (argc != 4)
				usage();
			return (do_create(argv[3], argv[2]));
		case 'd':
			if (argc != 4)
				usage();
			return (do_delete(argv[3], argv[2]));
		case 'n':
			if (argc != 5)
				usage();
			return (do_rename(argv[4], argv[2], argv[3]));
		case 's':
			if (argc != 5)
				usage();
			return (do_mount(argv[3], argv[2], argv[4]));
		case 'r':
			if (argc != 4)
				usage();
			return (do_revert(argv[3], argv[2]));
		default:
			usage();
	}

	return (0);
}
