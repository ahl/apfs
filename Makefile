#
# Copyright 2017 Adam H. Leventhal. All Rights Reserved.
#
 
all: snapUtil

snapUtil: snapUtil.c
	clang -Wall -Os -g -o snapUtil snapUtil.c -framework CoreFoundation -framework IOKit

clean:
	rm -f snapUtil
