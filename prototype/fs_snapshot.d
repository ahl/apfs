#!/usr/sbin/dtrace -s

/*
 * Copyright 2017 Adam H. Leventhal. All Rights Reserved.
 */

#pragma D option flowindent

/*
 * When a thread calls the fs_snapshot system call set a thread-local variable
 * called 'follow' to 1.
 */
syscall::fs_snapshot:entry
{
	self->follow = 1;
}

/*
 * For every function entry and return in the kernel (of which there are
 * many!) if the thread has its 'follow' value set, print out the first two
 * arguments (or the offset and return value for a return probe).
 */
fbt:::
/self->follow/
{
	printf("%x %x", arg0, arg1);
}

/*
 * When the thread returns from the fs_snapshot system call, set follow to 0
 * and exit this DTrace invocation (thus removing all instrumentation).
 */
syscall::fs_snapshot:return
/self->follow/
{
	self->follow = 0;
	exit(0);
}
