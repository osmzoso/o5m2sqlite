# o5m2sqlite
Converts openstreetmap data to sqlite database files.

Build with gcc:
gcc o5m2sqlite.c -lsqlite3 -o o5m2sqlite

(it requires the sqlite3 library being installed on your pc)

Usage:
./o5m2sqlite input.o5m output.sqlite
