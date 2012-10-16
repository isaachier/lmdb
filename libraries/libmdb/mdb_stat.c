/* mdb_stat.c - memory-mapped database status tool */
/*
 * Copyright 2011 Howard Chu, Symas Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mdb.h"

static void prstat(MDB_stat *ms)
{
	printf("Page size: %u\n", ms->ms_psize);
	printf("Tree depth: %u\n", ms->ms_depth);
	printf("Branch pages: %zu\n", ms->ms_branch_pages);
	printf("Leaf pages: %zu\n", ms->ms_leaf_pages);
	printf("Overflow pages: %zu\n", ms->ms_overflow_pages);
	printf("Entries: %zu\n", ms->ms_entries);
}

static void usage(char *prog)
{
	fprintf(stderr, "usage: %s dbpath [-a|-s subdb]\n", prog);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int i, rc;
	MDB_env *env;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_stat mst;
	MDB_envinfo mei;
	char *prog = argv[0];
	char *envname;
	char *subname = NULL;
	int alldbs = 0, envinfo = 0;

	if (argc < 2) {
		usage(prog);
	}

	/* -a: print stat of main DB and all subDBs
	 * -s: print stat of only the named subDB
	 * -e: print env info
	 * (default) print stat of only the main DB
	 */
	while ((i = getopt(argc, argv, "aes:")) != EOF) {
		switch(i) {
		case 'a':
			alldbs++;
			break;
		case 'e':
			envinfo++;
			break;
		case 's':
			subname = optarg;
			break;
		default:
			fprintf(stderr, "%s: unrecognized option -%c\n", prog, optopt);
			usage(prog);
		}
	}

	if (optind != argc - 1)
		usage(prog);

	envname = argv[optind];
	rc = mdb_env_create(&env);

	if (alldbs || subname) {
		mdb_env_set_maxdbs(env, 4);
	}

	rc = mdb_env_open(env, envname, MDB_RDONLY, 0664);
	if (rc) {
		printf("mdb_env_open failed, error %d %s\n", rc, mdb_strerror(rc));
		goto env_close;
	}
	rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		printf("mdb_txn_begin failed, error %d %s\n", rc, mdb_strerror(rc));
		goto env_close;
	}

	if (envinfo) {
		rc = mdb_env_info(env, &mei);
		printf("Map size: %zu \n", mei.me_mapsize);
		printf("Last transaction ID: %zu\n", mei.me_last_txnid);
		printf("Last page used: %zu\n", mei.me_last_pgno);
		printf("Max readers: %u\n", mei.me_maxreaders);
		printf("Number of readers used: %u\n", mei.me_numreaders);
	}

	rc = mdb_open(txn, subname, 0, &dbi);
	if (rc) {
		printf("mdb_open failed, error %d %s\n", rc, mdb_strerror(rc));
		goto txn_abort;
	}
   
	rc = mdb_stat(txn, dbi, &mst);
	if (rc) {
		printf("mdb_stat failed, error %d %s\n", rc, mdb_strerror(rc));
		goto txn_abort;
	}
	prstat(&mst);

	if (alldbs) {
		MDB_cursor *cursor;
		MDB_val key;

		rc = mdb_cursor_open(txn, dbi, &cursor);
		if (rc) {
			printf("mdb_cursor_open failed, error %d %s\n", rc, mdb_strerror(rc));
			goto txn_abort;
		}
		while ((rc = mdb_cursor_get(cursor, &key, NULL, MDB_NEXT)) == 0) {
			char *str = malloc(key.mv_size+1);
			MDB_dbi db2;
			memcpy(str, key.mv_data, key.mv_size);
			str[key.mv_size] = '\0';
			rc = mdb_open(txn, str, 0, &db2);
			if (rc == MDB_SUCCESS)
				printf("\n%s\n", str);
			free(str);
			if (rc) continue;
			rc = mdb_stat(txn, db2, &mst);
			if (rc) {
				printf("mdb_stat failed, error %d %s\n", rc, mdb_strerror(rc));
				goto txn_abort;
			}
			prstat(&mst);
			mdb_close(env, db2);
		}
		mdb_cursor_close(cursor);
	}

	mdb_close(env, dbi);
txn_abort:
	mdb_txn_abort(txn);
env_close:
	mdb_env_close(env);

	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
