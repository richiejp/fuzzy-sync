// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 Richard Palethorpe <rpalethorpe@suse.com>
 */

#include <unistd.h>

#include "fuzzy_sync.h"

#define RECORD_LEN 128

static char *record_path;
static struct fzsync_pair pair;
static FILE *record;
static volatile char winner;

static long long tons(struct timespec ts)
{
	long long res = ts.tv_sec;

	res *= 1000000000;
	res += ts.tv_nsec;

	return res;
}

static void setup(void)
{
	record = fopen(record_path, "w");

	if (!record) {
		fzsync_printf("fopen(%s, w) -> %s",
			      record_path, strerror(errno));
		exit(1);
	}

	if (fputs("winner,a_start,b_start,a_end,b_end\n", record) < 0
	    || fflush(record) != 0) {
		fzsync_printf("Can't write to %s -> %s",
			      record_path, strerror(errno));
		cleanup(1);
	}

	tst_fzsync_pair_init(&pair);
	pair.exec_loops = 100000;
}

static void *worker(void *v)
{
	struct timespec delay = { 0, 1 };

	while (fzsync_run_b(&pair)) {
		fzsync_start_race_b(&pair);
		nanosleep(&delay, NULL);
		winner = 'B';
		fzsync_end_race_b(&pair);
	}

	return v;
}

static void run(void)
{
	if (fzsync_pair_reset(&pair, worker))
		cleanup(1);

	while (fzsync_run_a(&pair)) {
		winner = 'A';

		fzsync_start_race_a(&pair);
		if (winner == 'A' && winner == 'B')
			winner = 'A';
		fzsync_end_race_a(&pair);

		fprintf(record, "%c,%lld,%lld,%lld,%lld\n", winner,
			tons(pair.a_start), tons(pair.b_start),
			tons(pair.a_end), tons(pair.b_end));
	}
}

static void cleanup(int exitno)
{
	fzsync_pair_cleanup(&pair);
	fclose(record);
	exit(exitno);
}

static void main(int argc, char *argv[])
{
	int opt;

	opt = getopt(argc, argv, "f:");

	if (opt != 'f') {
		fzsync_printf("Usage: %s -f <path>\n", argv[0]);
		return 1;
	}

	record_path = optarg;

	setup();
	run();
	cleanup(0);
}
