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
#include <sys/snapshot.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

const char *g_pname;

void
usage(void)
{
	(void) fprintf(stderr, "Usage:\n");
	(void) fprintf(stderr, "\t%s -l <vol>\t\t\t(List all snapshots)\n", g_pname);
	(void) fprintf(stderr, "\t%s -c <snap> <vol>\t\t(Create snapshot)\n", g_pname);
	(void) fprintf(stderr, "\t%s -n <snap> <newname> <vol>\t(Rename snapshot)\n", g_pname);
	(void) fprintf(stderr, "\t%s -d <snap> <vol>\t\t(Delete snapshot)\n", g_pname);
	(void) fprintf(stderr, "\t%s -r <snap> <vol>\t\t(Revert to snapshot)\n", g_pname);
	(void) fprintf(stderr, "\t%s -s <snap> <vol> <mntpnt>\t(Mount snapshot)\n", g_pname);
	(void) fprintf(stderr, "\t%s -o\t\t\t\t(Print original snapshot name)\n", g_pname);
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

	for (;;) {
		int count = fs_snapshot_list(dirfd, &alist, &abuf[0], sizeof (abuf), 0);
		if (count < 0) {
			perror("fs_snapshot_list");
			exit(1);
		} else if (count == 0) {
			break;
		} else {
			char *p = &abuf[0];
			for (int i = 0; i < count; i++) {
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
		}
	}

	return (0);
}

int
do_origName(void) {
	const UInt8 *bytes;
	CFIndex length;
	CFDataRef manifestHash, rootSnapshotName;

	io_registry_entry_t chosen = IORegistryEntryFromPath(0, "IODeviceTree:/chosen");

	rootSnapshotName = IORegistryEntryCreateCFProperty(chosen, CFSTR("root-snapshot-name"), kCFAllocatorDefault, 0);

	if (rootSnapshotName != NULL && CFGetTypeID(rootSnapshotName) == CFDataGetTypeID()) {
		CFStringRef snapshotString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, rootSnapshotName, kCFStringEncodingUTF8);
		CFRelease(rootSnapshotName);
		char buffer[100];
		const char *ptr = CFStringGetCStringPtr(snapshotString, kCFStringEncodingUTF8);
		if (ptr == NULL) {
			if (CFStringGetCString(snapshotString, buffer, 100, kCFStringEncodingUTF8))
				ptr = buffer;
		}
		printf("%s\n", ptr);
	} else {
		manifestHash = (CFDataRef)IORegistryEntryCreateCFProperty(chosen, CFSTR("boot-manifest-hash"), kCFAllocatorDefault, 0);
		IOObjectRelease(chosen);

		if (manifestHash == NULL || CFGetTypeID(manifestHash) != CFDataGetTypeID()) {
			fprintf(stderr, "Unable to read boot-manifest-hash or root-snapshot-name\n");
			return 1;
		}

		length = CFDataGetLength(manifestHash);
		bytes = CFDataGetBytePtr(manifestHash);
		CFRelease(manifestHash);

		printf("com.apple.os.update-");
		for (int i = 0; i < length; i++)
			printf("%02X", bytes[i]);
		printf("\n");
	}

	return 0;
}

int
main(int argc, char **argv)
{
	g_pname = argv[0];

	if (argc < 2 || argv[1][0] != '-' ||
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
		case 'o':
			if (argc != 2)
				usage();
			return (do_origName());
		default:
			usage();
	}

	return (0);
}
