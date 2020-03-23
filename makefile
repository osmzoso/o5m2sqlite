#
# Makefile
#
# Build with gcc for Windows
o5m2sqlite: o5m2sqlite.c o5mreader.c o5mreader.h sqlite3.c sqlite3.h
	gcc -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_ENABLE_RTREE o5m2sqlite.c sqlite3.c -O2 -o o5m2sqlite -s

# Build with gcc for Linux
#	gcc o5m2sqlite.c sqlite3.c -lpthread -ldl -O2 -o o5m2sqlite
