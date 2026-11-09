/*
 * Tests of the writer C ABI: v1 must keep working, and every v2 function
 * must leave the expected rows in the database. Written in C on purpose, so
 * the header is checked as a C header.
 *
 * Usage: sodb_abi_test <scratch.srctrldb>
 */

#include "sourcetrail_odin_writer.h"

#include "sqlite3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                       \
	do                                                                    \
	{                                                                     \
		if (!(cond))                                                      \
		{                                                                 \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                   \
		}                                                                 \
	} while (0)

static sqlite3* db = NULL;

/* Runs a query that yields one integer; -1 if it yields no row. */
static long long scalar(const char* sql)
{
	sqlite3_stmt* stmt = NULL;
	long long value = -1;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
	{
		fprintf(stderr, "bad SQL: %s (%s)\n", sql, sqlite3_errmsg(db));
		++failures;
		return -1;
	}
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		value = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return value;
}

static long long scalarf(const char* format, long long a)
{
	char sql[512];
	snprintf(sql, sizeof(sql), format, a);
	return scalar(sql);
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: sodb_abi_test <scratch.srctrldb>\n");
		return 2;
	}
	const char* path = argv[1];
	remove(path);

	CHECK(sodb_abi_version() == SODB_ABI_VERSION);
	CHECK(sodb_abi_version() == 2);

	sodb_writer w = sodb_open(path);
	CHECK(w != NULL);
	if (!w)
	{
		return 1;
	}
	CHECK(sodb_begin(w) == 1);

	const int file = sodb_file(w, "abi_test.src");
	CHECK(file > 0);
	CHECK(sodb_file_language(w, file, "test") == 1);

	/* v1: the name is split on the delimiter and recorded as explicit. */
	const int v1 = sodb_symbol(w, "pkg.A", ".", SODB_SYMBOL_STRUCT);
	CHECK(v1 > 0);
	CHECK(sodb_symbol_location(w, v1, file, 1, 1, 1, 5) == 1);

	/* v2: two overloads differ only in the postfix of the last level. */
	sodb_name_element f_int[2] = {{NULL, "C", NULL}, {"void", "f", "(int)"}};
	sodb_name_element f_str[2] = {{NULL, "C", NULL}, {"void", "f", "(string)"}};
	const int fi = sodb_symbol_ex(w, "::", f_int, 2, SODB_SYMBOL_METHOD, SODB_DEFINITION_EXPLICIT);
	const int fs = sodb_symbol_ex(w, "::", f_str, 2, SODB_SYMBOL_METHOD, SODB_DEFINITION_EXPLICIT);
	CHECK(fi > 0);
	CHECK(fs > 0);
	CHECK(fi != fs);
	/* The same name again is the same symbol. */
	CHECK(sodb_symbol_ex(w, "::", f_int, 2, SODB_SYMBOL_METHOD, SODB_DEFINITION_EXPLICIT) == fi);

	/* A referenced-only symbol stays non-indexed. */
	sodb_name_element ext[2] = {{NULL, "std", NULL}, {NULL, "Vec", NULL}};
	const int vec = sodb_symbol_ex(w, "::", ext, 2, SODB_SYMBOL_STRUCT, SODB_DEFINITION_NONE);
	CHECK(vec > 0);

	/* Implicit first, explicit later: the later kind wins. */
	sodb_name_element late[1] = {{NULL, "late", NULL}};
	const int lt = sodb_symbol_ex(w, "::", late, 1, SODB_SYMBOL_FUNCTION, SODB_DEFINITION_IMPLICIT);
	CHECK(lt > 0);
	CHECK(sodb_symbol_ex(w, "::", late, 1, SODB_SYMBOL_KIND_NONE, SODB_DEFINITION_EXPLICIT) == lt);

	/* Bad arguments fail with a message and record nothing. */
	sodb_name_element empty[1] = {{NULL, "", NULL}};
	CHECK(sodb_symbol_ex(w, "::", empty, 1, SODB_SYMBOL_FUNCTION, SODB_DEFINITION_EXPLICIT) == 0);
	CHECK(strstr(sodb_last_error(w), "empty") != NULL);
	CHECK(sodb_symbol_ex(w, "", f_int, 2, SODB_SYMBOL_METHOD, SODB_DEFINITION_EXPLICIT) == 0);
	CHECK(sodb_symbol_ex(w, "::", f_int, 0, SODB_SYMBOL_METHOD, SODB_DEFINITION_EXPLICIT) == 0);
	CHECK(sodb_symbol_ex(w, "::", NULL, 1, SODB_SYMBOL_METHOD, SODB_DEFINITION_EXPLICIT) == 0);
	CHECK(sodb_symbol_ex(w, "::", f_int, 2, SODB_SYMBOL_UNION + 1, SODB_DEFINITION_EXPLICIT) == 0);
	CHECK(sodb_symbol_ex(w, "::", f_int, 2, SODB_SYMBOL_METHOD, 3) == 0);
	CHECK(strstr(sodb_last_error(w), "definition kind") != NULL);
	CHECK(sodb_local_symbol(w, "") == 0);
	/* A later valid call clears the shim error. */
	CHECK(sodb_file(w, "abi_test.src") == file);
	CHECK(strstr(sodb_last_error(w), "sodb_") == NULL);

	/* Locations of every v2 kind. */
	CHECK(sodb_symbol_location(w, fi, file, 3, 6, 3, 6) == 1);
	CHECK(sodb_symbol_signature_location(w, fi, file, 3, 1, 3, 12) == 1);
	CHECK(sodb_qualifier_location(w, v1, file, 7, 1, 7, 1) == 1);
	CHECK(sodb_atomic_source_range(w, file, 10, 1, 12, 2) == 1);

	/* An ambiguous call. */
	const int call = sodb_reference(w, fi, fs, SODB_REFERENCE_CALL);
	CHECK(call > 0);
	CHECK(sodb_reference_location(w, call, file, 4, 3, 4, 3) == 1);
	CHECK(sodb_reference_ambiguous(w, call) == 1);

	/* Locals: the name is the identity. */
	const int local = sodb_local_symbol(w, "abi_test.src:4:7");
	CHECK(local > 0);
	CHECK(sodb_local_symbol(w, "abi_test.src:4:7") == local);
	CHECK(sodb_local_symbol_location(w, local, file, 4, 7, 4, 7) == 1);

	CHECK(sodb_commit(w) == 1);
	sodb_close(w);

	/* Read the database back. */
	CHECK(sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);

	/* Serialized names: <delimiter>\tm then name\tsprefix\tppostfix per level. */
	CHECK(scalar("SELECT count(*) FROM node WHERE serialized_name = "
				 "'::'||char(9)||'mC'||char(9)||'s'||char(9)||'p'||char(9)||'nf'||char(9)||'svoid'||char(9)||'p(int)'") == 1);
	CHECK(scalar("SELECT count(*) FROM node WHERE serialized_name = "
				 "'::'||char(9)||'mC'||char(9)||'s'||char(9)||'p'||char(9)||'nf'||char(9)||'svoid'||char(9)||'p(string)'") == 1);
	CHECK(scalar("SELECT count(*) FROM node WHERE serialized_name = "
				 "'.'||char(9)||'mpkg'||char(9)||'s'||char(9)||'p'||char(9)||'nA'||char(9)||'s'||char(9)||'p'") == 1);

	/* Definition kinds: explicit = 2, implicit = 1, none = no row. */
	CHECK(scalarf("SELECT definition_kind FROM symbol WHERE id = %lld", v1) == 2);
	CHECK(scalarf("SELECT definition_kind FROM symbol WHERE id = %lld", fi) == 2);
	CHECK(scalarf("SELECT count(*) FROM symbol WHERE id = %lld", vec) == 0);
	CHECK(scalarf("SELECT definition_kind FROM symbol WHERE id = %lld", lt) == 2);

	/* Node types follow the symbol kind; KIND_NONE kept the earlier kind. */
	CHECK(scalarf("SELECT type FROM node WHERE id = %lld", fi) ==
		  scalarf("SELECT type FROM node WHERE id = %lld", fs));
	CHECK(scalarf("SELECT type FROM node WHERE id = %lld", lt) != 1);

	/* Location types: 0 token, 2 qualifier, 3 local, 4 signature, 5 atomic. */
	CHECK(scalarf("SELECT count(*) FROM occurrence o JOIN source_location l ON l.id = o.source_location_id "
				  "WHERE o.element_id = %lld AND l.type = 4", fi) == 1);
	CHECK(scalarf("SELECT count(*) FROM occurrence o JOIN source_location l ON l.id = o.source_location_id "
				  "WHERE o.element_id = %lld AND l.type = 2", v1) == 1);
	CHECK(scalarf("SELECT count(*) FROM occurrence o JOIN source_location l ON l.id = o.source_location_id "
				  "WHERE o.element_id = %lld AND l.type = 3", local) == 1);
	CHECK(scalar("SELECT count(*) FROM source_location WHERE type = 5 AND start_line = 10 AND end_line = 12") == 1);

	/* The ambiguous call: an edge of type call (8) with the component. */
	CHECK(scalarf("SELECT type FROM edge WHERE id = %lld", call) == 8);
	CHECK(scalarf("SELECT count(*) FROM element_component WHERE element_id = %lld AND type = 1", call) == 1);

	/* The local symbol and nothing recorded by the failed calls. */
	CHECK(scalar("SELECT count(*) FROM local_symbol WHERE name = 'abi_test.src:4:7'") == 1);
	CHECK(scalar("SELECT count(*) FROM local_symbol") == 1);
	CHECK(scalar("SELECT count(*) FROM error") == 0);

	sqlite3_close(db);

	if (failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("sodb_abi_test: all checks passed\n");
	return 0;
}
