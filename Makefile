#
# Copyright 2017 Adam H. Leventhal. All Rights Reserved.
#
 
all: snapUtil

snapUtil: snapUtil.c
	clang -o snapUtil snapUtil.c

clean:
	rm -f snapUtil
