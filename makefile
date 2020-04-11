#
# Makefile
#

# Dependencies
o5m2sqlite: o5m2sqlite.c o5mreader.c o5mreader.h sqlite3.c sqlite3.h

# Build with gcc for Linux
	gcc -O2 -s -DSQLITE_ENABLE_RTREE o5m2sqlite.c sqlite3.c -lpthread -ldl -o o5m2sqlite

