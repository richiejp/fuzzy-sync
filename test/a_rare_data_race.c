// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 Richard Palethorpe <rpalethorpe@suse.com>
 */

#include <stdlib.h>
#include <stdio.h>

#include "tst_test.h"
#include "tst_safe_stdio.h"
#include "tst_fuzzy_sync.h"

#define RECORD_LEN 128

static char *record_path;
static struct tst_option opts[] = {
	{"f:", &record_path, "-f PATH	Path to record file"},
	{NULL, NULL, NULL}
};

static struct tst_fzsync_pair pair;
static FILE *record;
static volatile char winner;

static long long tons(struct timespec ts)
{
	return tst_ts_to_ns(tst_ts_from_timespec(ts));
}

static void setup(void)
{
	record = SAFE_FOPEN(record_path, "w");

	if (fputs("winner,a_start,b_start,a_end,b_end\n", record) < 0 || fflush(record) != 0)
		tst_brk(TBROK | TERRNO, "Can't write to %s", record_path);

	tst_fzsync_pair_init(&pair);
	pair.exec_loops = 100000;
}

static void *worker(void *v LTP_ATTRIBUTE_UNUSED)
{
	struct timespec delay = { 0, 1 };

	while (tst_fzsync_run_b(&pair)) {
		tst_fzsync_start_race_b(&pair);
		nanosleep(&delay, NULL);
		winner = 'B';
		tst_fzsync_end_race_b(&pair);
	}

	return NULL;
}

static void run(void)
{
	tst_fzsync_pair_reset(&pair, worker);

	while (tst_fzsync_run_a(&pair)) {
		winner = 'A';

		tst_fzsync_start_race_a(&pair);
		if (winner == 'A' && winner == 'B')
			winner = 'A';
		tst_fzsync_end_race_a(&pair);

		fprintf(record, "%c,%lld,%lld,%lld,%lld\n", winner,
			tons(pair.a_start), tons(pair.b_start),
			tons(pair.a_end), tons(pair.b_end));
	}

	tst_res(TPASS, "We made it to the end!");
}

static void cleanup(void)
{
	tst_fzsync_pair_cleanup(&pair);
	fclose(record);
}

static struct tst_test test = {
	.setup = setup,
	.options = opts,
	.cleanup = cleanup,
	.test_all = run,
};
