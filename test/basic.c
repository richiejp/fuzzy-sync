// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Richard Palethorpe <rpalethorpe@suse.com>
 */
/*\
 * [DESCRIPTION]
 *
 * This verifies Fuzzy Sync's basic ability to reproduce a particular
 * outcome to a data race when the critical sections are not aligned.
 *
 * We make the simplifying assumptions that:
 * - Each thread contains a single contiguous critical section.
 * - The threads only interact through a single variable.
 * - The various timings are constant except for variations introduced
 *   by the environment.
 *
 * If a single data race has N critical sections then we may remove
 * N-1 sections to produce a more difficult race. We may then test
 * only the more difficult race and induce from this the outcome of
 * testing the easier races.
 *
 * In real code, the threads may interact through many side
 * effects. While some of these side effects may not result in a bug,
 * they may effect the total time it takes to execute either
 * thread. This will be handled in multi.
 *
 * The number of variables which two threads interact through is
 * irrelevant as the combined state of two variables can be
 * represented with a single variable. We may also reduce the number
 * of states to simply those required to show the thread is inside or
 * outside of the critical section.
 *
 * There are two fundamental races which require alignment under these
 * assumptions:
 *      1        2
 * A +-----+  +----+	The outer box is total execution time.
 *   | #   |  | #  |	The '#' is the critical section.
 *
 *   |  # |   |   # |
 * B +----+   +-----+
 *
 * So we can either have the critical section of the shorter race
 * before that of the longer one. Or the critical section of the
 * longer one before the shorter.
 *
 * In reality both threads will never be the same length, but we can
 * test that anyway. We also test with both A as the shorter and B as
 * the shorter. We also vary the distance of the critical section from
 * the start or end. The delay times are cubed to ensure that a delay
 * range is required.
 *
 * When entering their critical sections, both threads increment the
 * 'c' counter variable atomically. They both also increment it when
 * leaving their critical sections. We record the value of 'c' when A
 * increments it. From the recorded values of 'c' we can deduce if the
 * critical sections overlap and their ordering.
 *
 * 	Start (cs)	| End (ct)	| Ordering
 * 	--------------------------------------------
 * 	1		| 2		| A before B
 * 	3		| 4		| B before A
 *
 * Any other combination of 'cs' and 'ct' means the critical sections
 * overlapped.
\*/

#include "fuzzy_sync.h"

#ifndef DEBUG
# define DEBUG 0
#endif

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* Scale all the delay times by this function. The races become harder
 * the faster this function grows. With cubic scaling the race windows
 * will be 27 times smaller than the entry or return delays. Because
 * TIME_SCALE(1) = 1*1*1, TIME_SCALE(3) = 3*3*3.
 */
#define TIME_SCALE(x) ((x) * (x) * (x))

/* The time signature of a code path containing a critical section. */
struct window {
	/* The delay until the start of the critical section */
	const int critical_s;
	/* The length of the critical section */
	const int critical_t;
	/* The remaining delay until the method returns */
	const int return_t;
};

/* The time signatures of threads A and B */
struct race {
	const struct window a;
	const struct window b;
};

static int c;
static struct fzsync_pair pair;

static const struct race races[] = {
	/* Degnerate cases where the critical sections are already
	 * aligned. The first case will fail when ncpu < 2 and yield
	 * is disabled.
	 */
	{ .a = { 0, 0, 0 }, .b = { 0, 0, 0 } },
	{ .a = { 0, 1, 0 }, .b = { 0, 1, 0 } },
	{ .a = { 1, 1, 1 }, .b = { 1, 1, 1 } },
	{ .a = { 3, 1, 1 }, .b = { 3, 1, 1 } },

	/* Both windows are the same length */
	{ .a = { 3, 1, 1 }, .b = { 1, 1, 3 } },
	{ .a = { 1, 1, 3 }, .b = { 3, 1, 1 } },

	/* Different sized windows */
	{ .a = { 3, 1, 1 }, .b = { 1, 1, 2 } },
	{ .a = { 1, 1, 3 }, .b = { 2, 1, 1 } },
	{ .a = { 2, 1, 1 }, .b = { 1, 1, 3 } },
	{ .a = { 1, 1, 2 }, .b = { 3, 1, 1 } },

	/* Same as above, but with critical section at entry or exit */
	{ .a = { 3, 1, 0 }, .b = { 0, 1, 3 } },
	{ .a = { 0, 1, 3 }, .b = { 3, 1, 0 } },

	{ .a = { 3, 1, 0 }, .b = { 0, 1, 2 } },
	{ .a = { 0, 1, 3 }, .b = { 2, 1, 0 } },
	{ .a = { 2, 1, 0 }, .b = { 0, 1, 3 } },
	{ .a = { 0, 1, 2 }, .b = { 3, 1, 0 } },

	/* One side is very short */
	{ .a = { 3, 1, 1 }, .b = { 0, 1, 0 } },
	{ .a = { 1, 1, 3 }, .b = { 0, 1, 0 } },
	{ .a = { 0, 1, 0 }, .b = { 1, 1, 3 } },
	{ .a = { 0, 1, 0 }, .b = { 3, 1, 1 } },

	{ .a = { 3, 1, 1 }, .b = { 0, 0, 0 } },
	{ .a = { 1, 1, 3 }, .b = { 0, 0, 0 } },
	{ .a = { 0, 0, 0 }, .b = { 1, 1, 3 } },
	{ .a = { 0, 0, 0 }, .b = { 3, 1, 1 } },

};

static void cleanup(void)
{
	fzsync_pair_cleanup(&pair);
}

static void setup(void)
{
	pair.min_samples = 10000;

	fzsync_pair_init(&pair);
}

static void delay(const int t)
{
	int k = TIME_SCALE(t);

	while (fzsync_atomic_add_return(-1, &k) > 0)
		sched_yield();
}

static void *worker(void *v)
{
	unsigned int i = *(unsigned int *)v;
	const struct window b = races[i].b;
	struct timespec s_time, window_s_time, window_t_time;
	struct fzsync_stat s = { 0 }, t = { 0 };


	while (fzsync_run_b(&pair)) {

		fzsync_time(&s_time);
		fzsync_start_race_b(&pair);

		delay(b.critical_s);

		fzsync_atomic_add_return(1, &c);
		fzsync_time(&window_s_time);
		delay(b.critical_t);
		fzsync_atomic_add_return(1, &c);
		fzsync_time(&window_t_time);

		delay(b.return_t);
		fzsync_end_race_b(&pair);

		fzsync_upd_diff_stat(&s, 0.25, window_s_time, s_time);
		fzsync_upd_diff_stat(&t, 0.25, window_t_time, s_time);

		if (!DEBUG || (pair.exec_loop != 5000 && pair.exec_loop % 100000 > 0))
			continue;

		fzsync_stat_info(s, "ns", "B window start");
		fzsync_stat_info(t, "ns", "B window end");
	}

	return NULL;
}

static void run(unsigned int i)
{
	const struct window a = races[i].a;
	struct fzsync_run_thread wrap_run_b = {
		.func = worker,
		.arg = &i,
	};
	int rval;
	int cs, ct, r, too_early = 0, critical = 0, too_late = 0;
	struct timespec s_time, window_s_time, window_t_time;
	struct fzsync_stat s = { 0 }, t = { 0 };

	fzsync_pair_reset(&pair, NULL);
	rval = pthread_create(&pair.thread_b, 0, fzsync_thread_wrapper,
			      &wrap_run_b);
	if (rval) {
		fzsync_printf("pthread_create: %s", strerror(rval));
		return;
	}

	while (fzsync_run_a(&pair)) {

		fzsync_time(&s_time);
		fzsync_start_race_a(&pair);
		delay(a.critical_s);

		fzsync_time(&window_s_time);
		cs = fzsync_atomic_add_return(1, &c);
		delay(a.critical_t);
		ct = fzsync_atomic_add_return(1, &c);
		fzsync_time(&window_t_time);

		delay(a.return_t);
		fzsync_end_race_a(&pair);

		if (cs == 1 && ct == 2)
			too_early++;
		else if (cs == 3 && ct == 4)
			too_late++;
		else
			critical++;

		r = fzsync_atomic_add_return(-4, &c);
		if (r) {
			fzsync_printf("cs = %d, ct = %d, r = %d", cs, ct, r);
			return;
		}

		fzsync_upd_diff_stat(&s, 0.25, window_s_time, s_time);
		fzsync_upd_diff_stat(&t, 0.25, window_t_time, s_time);

		if (critical > 100) {
			fzsync_pair_cleanup(&pair);
			break;
		}

		if (!DEBUG || (pair.exec_loop != 5000 && pair.exec_loop % 100000 > 0))
			continue;

		fzsync_stat_info(s, "ns", "A window start");
		fzsync_stat_info(t, "ns", "A window end");
	}

	fzsync_printf(
		"acs:%-2d act:%-2d art:%-2d | =:%-4d -:%-4d +:%-4d\n",
		a.critical_s, a.critical_t, a.return_t,
		critical, too_early, too_late);
}

int main(void)
{
	unsigned int i;

	setup();
	for (i = 0; i < ARRAY_SIZE(races); i++)
		run(i);
	cleanup();

	return 0;
}
