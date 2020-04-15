#
# Makefile
#

# Dependencies
o5m2sqlite: o5m2sqlite.c o5mreader.c o5mreader.h sqlite3.c sqlite3.h

# Build with gcc for Linux
	gcc -O2 -s -DSQLITE_ENABLE_RTREE o5m2sqlite.c sqlite3.c -lpthread -ldl -o o5m2sqlite

# Build with gcc for Windows
#	gcc -O2 -s -m64 -DSQLITE_OS_WIN=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_ENABLE_RTREE o5m2sqlite.c sqlite3.c -o o5m2sqlite
