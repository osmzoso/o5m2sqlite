#
# Makefile
#

# Dependencies
o5m2sqlite: o5m2sqlite.c o5mreader.c o5mreader.h sqlite3.c sqlite3.h

# Build with gcc for Linux
	gcc -DSQLITE_ENABLE_RTREE o5m2sqlite.c sqlite3.c -lpthread -ldl -O2 -o o5m2sqlite -s

# Build with gcc for Windows
#	gcc -DSQLITE_ENABLE_RTREE o5m2sqlite.c sqlite3.c -O2 -o o5m2sqlite -s

