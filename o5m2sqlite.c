/*
** o5m2sqlite
**
** Converts OpenStreetMap data in binary o5m format into a SQLite database
**
** Fork from https://github.com/Rotfuss/o5m2sqlite Tillmann Stuebler, 12 August 2016
** (based on the example in README.md from https://github.com/bigr/o5mreader)
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "o5mreader.c"
#include "sqlite3.h"

#define O5M2SQLITE_VERSION "0.3 alpha"

#define O5M2SQLITE_CREATE_TABLES \
"CREATE TABLE nodes (node_id INTEGER PRIMARY KEY,lat REAL,lon REAL);\n" \
"CREATE TABLE node_tags (node_id INTEGER,key TEXT,value TEXT);\n" \
"CREATE TABLE way_tags (way_id INTEGER,key TEXT,value TEXT);\n" \
"CREATE TABLE way_nodes (way_id INTEGER,local_order INTEGER,node_id INTEGER);\n" \
"CREATE TABLE relation_tags (relation_id INTEGER,key TEXT,value TEXT);\n" \
"CREATE TABLE relation_members (relation_id INTEGER,type TEXT,ref INTEGER,role TEXT,local_order INTEGER);\n"

#define O5M2SQLITE_CREATE_INDEXES \
"CREATE INDEX node_tags__node_id ON node_tags ( node_id );\n" \
"CREATE INDEX node_tags__key ON node_tags ( key );\n" \
"CREATE INDEX way_tags__way_id ON way_tags ( way_id );\n" \
"CREATE INDEX way_tags__key ON way_tags ( key );\n" \
"CREATE INDEX way_nodes__way_id ON way_nodes ( way_id );\n" \
"CREATE INDEX way_nodes__node_id ON way_nodes ( node_id );\n" \
"CREATE INDEX relation_tags__relation_id ON relation_tags ( relation_id );\n" \
"CREATE INDEX relation_tags__key ON relation_tags ( key );\n" \
"CREATE INDEX relation_members__relation_id ON relation_members ( relation_id );\n" \
"CREATE INDEX relation_members__type ON relation_members ( type, ref );\n\n" \
"-- Spatial R*Tree index on all ways with key='highway'\n" \
"CREATE VIRTUAL TABLE rtree_way_highway USING rtree( way_id,min_lat, max_lat,min_lon, max_lon );\n" \
"INSERT INTO rtree_way_highway (way_id,min_lat,       max_lat,       min_lon,       max_lon)\n" \
"SELECT                way_tags.way_id,min(nodes.lat),max(nodes.lat),min(nodes.lon),max(nodes.lon)\n" \
"FROM      way_tags\n" \
"LEFT JOIN way_nodes ON way_tags.way_id=way_nodes.way_id\n" \
"LEFT JOIN nodes     ON way_nodes.node_id=nodes.node_id\n" \
"WHERE way_tags.key='highway'\n" \
"GROUP BY way_tags.way_id;\n"

#define ins_node       "INSERT INTO nodes (node_id,lat,lon) VALUES (?1,?2,?3);"
#define ins_node_tag   "INSERT INTO node_tags (node_id,key,value) VALUES (?1,?2,?3);"
#define ins_way_tag    "INSERT INTO way_tags (way_id,key,value) VALUES (?1,?2,?3);"
#define ins_way_node   "INSERT INTO way_nodes (way_id,local_order,node_id) VALUES (?1,?2,?3);"
#define ins_rel_tag    "INSERT INTO relation_tags (relation_id,key,value) VALUES (?1,?2,?3);"
#define ins_rel_member "INSERT INTO relation_members (relation_id,type,ref,role) VALUES (?1,?2,?3,?4);"

#define O5M2SQLITE_HELP \
"o5m2sqlite (Version " O5M2SQLITE_VERSION ")\n\n" \
"Converts OpenStreetMap data in binary o5m format into a SQLite database.\n" \
"(SQLite Version " SQLITE_VERSION ")\n\n" \
"Usage:\n" \
"o5m2sqlite in.o5m out.sqlite3\tconvert in.o5m to out.sqlite3\n" \
"o5m2sqlite --schema\t\tshow the resulting sqlite database schema\n\n" \
"(compile time: " __DATE__ " " __TIME__ "  gcc " __VERSION__ ")\n"

/* sqlite db handler */
sqlite3 *db;

static void check_rc( int rc ) {
    if( rc!=SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
}

int main(int narg, char * arg[])
{
    // o5m reader
    O5mreader* reader;
    O5mreaderDataset ds;
    O5mreaderIterateRet ret, ret2;
    char *key, *val;
    uint64_t nodeId;
    uint64_t refId;
    uint8_t type;
    char *role;
    FILE * f;
    uint64_t local_order;
    uint64_t cnt_ds;
    
    // sqlite
    sqlite3_stmt *stmt_node, *stmt_node_tag, *stmt_way_tag, *stmt_way_node, *stmt_rel_tag, *stmt_rel_member;
    
    if((narg==2) && strcmp(arg[1],"--schema")==0) {
        fprintf(stderr, "\n%s\n%s\n\n", O5M2SQLITE_CREATE_TABLES, O5M2SQLITE_CREATE_INDEXES);
        return(0);
    }
    
    if(narg<3) {
        fprintf(stderr, O5M2SQLITE_HELP );
        return(1);
    }
    
    // open o5m file
    f = fopen(arg[1],"rb");
    if( f==NULL ) {
        fprintf(stderr, "Can't open o5m file %s\n", arg[1]);
        return(1);
    }
    
    // open sqlite database
    check_rc( sqlite3_open(arg[2], &db) );
    
    check_rc( sqlite3_exec(db,"PRAGMA synchronous = OFF",NULL,NULL,NULL) );
    check_rc( sqlite3_exec(db,"PRAGMA journal_mode = MEMORY",NULL,NULL,NULL) );
    
    check_rc( sqlite3_exec(db,"BEGIN TRANSACTION",NULL,NULL,NULL) );
    
    // create tables
    fprintf(stderr,"create tables...\n");
    check_rc( sqlite3_exec(db,O5M2SQLITE_CREATE_TABLES,NULL,NULL,NULL) );
    
    // prepare statements
    check_rc( sqlite3_prepare_v2(db,ins_node,-1,&stmt_node,NULL) );
    check_rc( sqlite3_prepare_v2(db,ins_node_tag,-1,&stmt_node_tag,NULL) );
    check_rc( sqlite3_prepare_v2(db,ins_way_tag,-1,&stmt_way_tag,NULL) );
    check_rc( sqlite3_prepare_v2(db,ins_way_node,-1,&stmt_way_node,NULL) );
    check_rc( sqlite3_prepare_v2(db,ins_rel_tag,-1,&stmt_rel_tag,NULL) );
    check_rc( sqlite3_prepare_v2(db,ins_rel_member,-1,&stmt_rel_member,NULL) );
    
    o5mreader_open(&reader,f);
    
    // iterate over the o5m file entries
    while( (ret = o5mreader_iterateDataSet(reader, &ds)) == O5MREADER_ITERATE_RET_NEXT ) {
        switch ( ds.type ) {
            // Data set is node
            case O5MREADER_DS_NODE:
                // Could do something with ds.id, ds.lon, ds.lat here, lon and lat are ints in 1E+7 * degree units
                sqlite3_bind_int64(stmt_node,1,ds.id);
                sqlite3_bind_double(stmt_node,2,ds.lat/1E7);
                sqlite3_bind_double(stmt_node,3,ds.lon/1E7);
                if(sqlite3_step(stmt_node)==SQLITE_DONE) sqlite3_reset(stmt_node);
                else {
                    printf("could not insert node.\n");
                    sqlite3_close(db);
                    return -6;
                }
                
                sqlite3_bind_int64(stmt_node_tag,1,ds.id);
                // Node tags iteration
                while ( (ret2 = o5mreader_iterateTags(reader,&key,&val)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with tag key and val
                    sqlite3_bind_text(stmt_node_tag,2,key,-1,NULL);
                    sqlite3_bind_text(stmt_node_tag,3,val,-1,NULL);
                    if(sqlite3_step(stmt_node_tag)==SQLITE_DONE) sqlite3_reset(stmt_node_tag);
                    else {
                        printf("could not insert node tag.\n");
                        sqlite3_close(db);
                        return -7;
                    }
                }
                break;
                
            // Data set is way
            case O5MREADER_DS_WAY:
                // Could do something with ds.id
                
                sqlite3_bind_int64(stmt_way_node,1,ds.id);
                // Nodes iteration
                local_order=0;
                while ( (ret2 = o5mreader_iterateNds(reader,&nodeId)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with nodeId
                    local_order++;
                    sqlite3_bind_int(stmt_way_node,2,local_order);
                    sqlite3_bind_int64(stmt_way_node,3,nodeId);
                    if(sqlite3_step(stmt_way_node)==SQLITE_DONE) sqlite3_reset(stmt_way_node);
                    else {
                        printf("could not insert way node.\n");
                        sqlite3_close(db);
                        return -9;
                    }
                }
                
                sqlite3_bind_int64(stmt_way_tag,1,ds.id);
                // Way tags iteration
                while ( (ret2 = o5mreader_iterateTags(reader,&key,&val)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with tag key and val
                    sqlite3_bind_text(stmt_way_tag,2,key,-1,NULL);
                    sqlite3_bind_text(stmt_way_tag,3,val,-1,NULL);
                    if(sqlite3_step(stmt_way_tag)==SQLITE_DONE) sqlite3_reset(stmt_way_tag);
                    else {
                        printf("could not insert way tag.\n");
                        sqlite3_close(db);
                        return -10;
                    }
                }
                break;
                
            // Data set is relation
            case O5MREADER_DS_REL:
                // Could do something with ds.id
                
                sqlite3_bind_int64(stmt_rel_member,1,ds.id);
                // Refs iteration
                while ( (ret2 = o5mreader_iterateRefs(reader,&refId,&type,&role)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with refId (way or node or rel id depends on type), type and role
                    
                    switch(type) {
                        case O5MREADER_DS_NODE:
                            sqlite3_bind_text(stmt_rel_member,2,"node",-1,NULL);
                            break;
                        case O5MREADER_DS_WAY:
                            sqlite3_bind_text(stmt_rel_member,2,"way",-1,NULL);
                            break;
                        case O5MREADER_DS_REL:
                            sqlite3_bind_text(stmt_rel_member,2,"relation",-1,NULL);
                            break;
                        default:
                            sqlite3_bind_text(stmt_rel_member,2,"",-1,NULL);
                            break;
                    }
                    sqlite3_bind_int64(stmt_rel_member,3,refId);
                    sqlite3_bind_text(stmt_rel_member,4,role,-1,NULL);
                    
                    if(sqlite3_step(stmt_rel_member)==SQLITE_DONE) sqlite3_reset(stmt_rel_member);
                    else {
                        printf("could not insert rel member.\n");
                        sqlite3_close(db);
                        return -12;
                    }
                }
                
                sqlite3_bind_int64(stmt_rel_tag,1,ds.id);
                // Relation tags iteration
                while ( (ret2 = o5mreader_iterateTags(reader,&key,&val)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with tag key and val
                    sqlite3_bind_text(stmt_rel_tag,2,key,-1,NULL);
                    sqlite3_bind_text(stmt_rel_tag,3,val,-1,NULL);
                    
                    if(sqlite3_step(stmt_rel_tag)==SQLITE_DONE) sqlite3_reset(stmt_rel_tag);
                    else {
                        printf("could not insert rel tag.\n");
                        sqlite3_close(db);
                        return -13;
                    }
                }
                break;
        } // end of switch-case
        
        cnt_ds++;
        if( cnt_ds>1000000 ) {
            fprintf(stderr,"o");
            cnt_ds = 0;
        }
    } // end of o5m elements iteration
    
    // close o5m file
    fclose(f);

    // finish transaction
    check_rc( sqlite3_exec(db,"COMMIT",NULL,NULL,NULL) );
    
    // create sqlite indexes
    fprintf(stderr,"\ncreate indexes...\n");
    check_rc( sqlite3_exec(db,O5M2SQLITE_CREATE_INDEXES,NULL,NULL,NULL) );
    
    // close sqlite database
    sqlite3_close(db);
    
    return 0;
}

