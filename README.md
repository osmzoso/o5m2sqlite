# o5m2sqlite

Converts OpenStreetMap data in binary o5m format into a SQLite database

Usage:  
./o5m2sqlite input.o5m output.sqlite3


## Tables in the database

    CREATE TABLE nodes (node_id INTEGER PRIMARY KEY,lat REAL,lon REAL);
    CREATE TABLE node_tags (node_id INTEGER,key TEXT,value TEXT);
    CREATE TABLE way_tags (way_id INTEGER,key TEXT,value TEXT);
    CREATE TABLE way_nodes (way_id INTEGER,local_order INTEGER,node_id INTEGER);
    CREATE TABLE relation_tags (relation_id INTEGER,key TEXT,value TEXT);
    CREATE TABLE relation_members (relation_id INTEGER,type TEXT,ref INTEGER,role TEXT,local_order INTEGER);


## Indexes

    CREATE INDEX node_tags_node_id ON node_tags ( node_id );
    CREATE INDEX node_tags_key ON node_tags ( key );
    CREATE INDEX way_tags_way_id ON way_tags ( way_id );
    CREATE INDEX way_tags_key ON way_tags ( key );
    CREATE INDEX way_nodes_way_id ON way_nodes ( way_id );
    CREATE INDEX way_nodes_node_id ON way_nodes ( node_id );
    CREATE INDEX relation_tags_relation_id ON relation_tags ( relation_id );
    CREATE INDEX relation_tags_key ON relation_tags ( key );
    CREATE INDEX relation_members_relation_id ON relation_members ( relation_id );
    CREATE INDEX relation_members_type ON relation_members ( type, ref );


## R*Tree index

    CREATE VIRTUAL TABLE rtree_way_highway USING rtree( way_id,min_lat, max_lat,min_lon, max_lon );
    INSERT INTO rtree_way_highway (way_id,min_lat,       max_lat,       min_lon,       max_lon)
    SELECT                way_tags.way_id,min(nodes.lat),max(nodes.lat),min(nodes.lon),max(nodes.lon)
    FROM      way_tags
    LEFT JOIN way_nodes ON way_tags.way_id=way_nodes.way_id
    LEFT JOIN nodes     ON way_nodes.node_id=nodes.node_id
    WHERE way_tags.key='highway'
    GROUP BY way_tags.way_id;


## Compiling

Required files in the same directory:  
_o5mreader.c_ _o5mreader.h_ from [https://github.com/bigr/o5mreader](https://github.com/bigr/o5mreader)  
_sqlite3.c_ _sqlite3.h_ from the sqlite-amalgamation [https://www.sqlite.org/download.html](https://www.sqlite.org/download.html)  

Build with gcc for Windows:

    gcc -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_ENABLE_RTREE o5m2sqlite.c sqlite3.c -O2 -o o5m2sqlite -s

Build with gcc for Linux:

    gcc o5m2sqlite.c sqlite3.c -lpthread -ldl -O2 -o o5m2sqlite
