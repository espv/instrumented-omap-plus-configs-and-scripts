/* Test library for the Linux Scheduler Simulator
 *
 * Basic assertions about tasks, ability to expect test failures.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYING); if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "test_lib.h"
#include <stdio.h>

void abort(void);

static long test_duration;
static const char *test_name;

/* Set to:
   0 if we expect the next test to pass
   1 if we expect the next test to fail
   -1 if we are in the middle of an aggregate test which we expect to fail
*/
int __expect_failure;
/* The number of expected failures which have occurred */
static int expected_failures;

#define NSEC(ms) ((ms) * (u64)NSEC_PER_MSEC)

/* check that max(0, expect - delta) <= v <= expect + delta */
int check_delta(u64 v, u64 expect, u64 delta) {
    if(delta < expect) {
        if(v + delta < expect) {
            return 0;
        }
    }
    return v <= expect + delta;
}

/* Given the pass/failure status of the most recent test, return true
 * if this is a reason to print a message and abort */
int check_unexpected(int condition) {
    if(__expect_failure == -1) {
        if(condition) {
            expected_failures++;
        }
        return 0;
    }
    if(__expect_failure) {
        if(condition) {
            expected_failures++;
            __expect_failure = 0;
        }
        return !condition;
    } else {
        return condition;
    }
}

int create_task(unsigned long mask, int sleep, int run) {
    struct cpumask aff;
    int p;

    cpumask_clear(&aff);
    memcpy(&aff, &mask, sizeof(mask));
    p = linsched_create_normal_task(linsched_create_sleep_run(sleep, run), 0);
    set_cpus_allowed(__linsched_tasks[p], aff);
    return p;
}

void create_tasks(unsigned int count, unsigned long mask, int sleep, int run) {
    unsigned int i;
    for(i = 0; i < count; i++) {
        create_task(mask, sleep, run);
    }
}

void expect_task_all(int pid, u64 runtime, u64 runtime_d,
                     u64 wait, u64 wait_d, unsigned int pcount, unsigned int pcount_d) {
    struct task_struct *t = __linsched_tasks[pid];
    if(check_unexpected(
           !check_delta(t->se.sum_exec_runtime, NSEC(runtime), NSEC(runtime_d)) ||
           !check_delta(t->sched_info.run_delay, NSEC(wait), NSEC(wait_d)) ||
           !check_delta(t->sched_info.pcount, pcount, pcount_d))) {
        if(__expect_failure) {
            puts("Unexpected success:");
        }
        printf("expected process %d to have:\n", pid);
        printf("runtime: %llu ms +- %llu ms\n", runtime, runtime_d);
        printf("run_delay: %llu ms +- %llu ms\n", wait, wait_d);
        printf("pcount: %d +- %d\n", pcount, pcount_d);
        puts("-----------------");
        linsched_print_task_stats();
        abort();
    }
}

void expect_tasks_all(int lowpid, int highpid, u64 runtime, u64 runtime_d,
                      u64 wait, u64 wait_d, unsigned int pcount, unsigned int pcount_d) {
    int i;
    int prev_fails = expected_failures;
    if(__expect_failure) {
        __expect_failure = -1;
    }
    for(i = lowpid; i <= highpid; i++) {
        expect_task_all(i, runtime, runtime_d, wait, wait_d, pcount, pcount_d);
    }
    if(__expect_failure) {
        if(expected_failures == prev_fails) {
            puts("Unexpected success:");
            printf("expected processes %d-%d to have:\n", lowpid, highpid);
            printf("runtime: %llu ms +- %llu ms\n", runtime, runtime_d);
            printf("run_delay: %llu ms +- %llu ms\n", wait, wait_d);
            printf("pcount: %d +- %d\n", pcount, pcount_d);
            puts("-----------------");
            linsched_print_task_stats();
            abort();
        }
        expected_failures = prev_fails + 1;
        __expect_failure = 0;
    }
}

void validate_results(int* expected_results) {
	expect_tasks_all(expected_results[0], expected_results[1],
				expected_results[2], expected_results[3],
				expected_results[4], expected_results[5],
				expected_results[6], expected_results[7]);
}

void set_test_duration(long ticks) {
    test_duration = ticks;
}

void set_test_name(const char *name) {
    test_name = name;
}

void __expect(const char *file, int line, const char *function,
              const char *cond, int succeeded){
    if(check_unexpected(!succeeded)) {
        if(__expect_failure) {
            puts("Unexpected success:");
        }
        printf("%s:%d: %s Check `%s'\n", file, line, function, cond);
        abort();
    }
}

__attribute__((weak)) void test_main(int argc, char **argv) {
    puts("implement tesk_main or main");
    abort();
}

__attribute__((weak)) int main(int argc, char **argv) {
    test_name = argv[0];
    test_main(argc, argv);
    if(expected_failures) {
        printf("test %s had %d expected failures\n", test_name, expected_failures);
    }
    return 0;
}
