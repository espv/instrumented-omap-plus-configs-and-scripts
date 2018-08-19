/* Basic test suite for the Linux Scheduler Simulator
 *
 * A basic set of automated tests for linsched. The most important
 * metric here is probably runtime, both run_delay and pcount are
 * (even) more susceptible to small changes in timing that would not
 * matter on a real machine.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYING); if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "linsched.h"
#include "test_lib.h"
#include <strings.h>
#include <stdio.h>

/* one minute */
#define TEST_TICKS 60000

struct linsched_topology topo_db[MAX_TOPOLOGIES] = {
	TOPO_UNIPROCESSOR,
	TOPO_DUAL_CPU,
	TOPO_DUAL_CPU_MC,
	TOPO_QUAD_CPU,
	TOPO_QUAD_CPU_MC,
	TOPO_QUAD_CPU_DUAL_SOCKET,
	TOPO_QUAD_CPU_QUAD_SOCKET,
	TOPO_HEX_CPU_DUAL_SOCKET_SMT
};

int parse_topology(char *arg) {
	if (!strcmp(arg, "uniprocessor"))
		return UNIPROCESSOR;
	else if (!strcmp(arg, "dual_cpu"))
		return DUAL_CPU;
	else if (!strcmp(arg, "dual_cpu_mc"))
		return DUAL_CPU_MC;
	else if (!strcmp(arg, "quad_cpu"))
		return QUAD_CPU;
	else if (!strcmp(arg, "quad_cpu_mc"))
		return QUAD_CPU_MC;
	else if (!strcmp(arg, "quad_cpu_dual_socket"))
		return QUAD_CPU_DUAL_SOCKET;
	else if (!strcmp(arg, "quad_cpu_quad_socket"))
		return QUAD_CPU_QUAD_SOCKET;
	else if (!strcmp(arg, "hex_cpu_dual_socket_smt"))
		return HEX_CPU_DUAL_SOCKET_SMT;
	return UNIPROCESSOR;
}

int* initialize_expectations(int *results, int result1, int result2,
				int result3, int result4, int result5,
				int result6, int result7, int result8) {
	results[0] = result1;
	results[1] = result2;
	results[2] = result3;
	results[3] = result4;
	results[4] = result5;
	results[5] = result6;
	results[6] = result7;
	results[7] = result8;
	return results;
}

extern void exit(int status);

/* ensure that we get a task running on each cpu without overhead */
void test_trivial_bal(int argc, char **argv) {
	int count, mask;
	struct linsched_topology topo;
	int expected_results[MAX_RESULTS];
	int type = parse_topology(argv[2]);

	topo = topo_db[type];
	count = topo.nr_cpus;
	mask = (1 << count) - 1;

	linsched_init(&topo);
	create_tasks(count, mask, 0, 100);
	linsched_run_sim(TEST_TICKS);

	/* For 12 core SMT, the current load distribution across domains
	 * isn't disparate enough to have a completely even distribution.
	 * One of the CPUs remains compeletely unused in this case.
	 */
	if (type == HEX_CPU_DUAL_SOCKET_SMT)
		expect_failure();
	validate_results(initialize_expectations((int*)expected_results,
			1, count, TEST_TICKS, 1, 0, 1, 1, 0));
}

/* make sure that we can run > nr_cpus sleep/run tasks which require
 * <= nr_cpus actual runtime without too much overhead */
void test_basic_bal1(int argc, char **argv) {
	int count, mask;
	struct linsched_topology topo;
	int type = parse_topology(argv[2]);
	int expected_results[MAX_RESULTS];

	topo = topo_db[type];

	switch (type) {
		case UNIPROCESSOR:
			count = topo.nr_cpus + 1;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 24000, 10, 24000, 10, 6000, 10);
			break;
		case DUAL_CPU:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 25000, 100, 22400, 100, 4900, 100);
			break;
		case DUAL_CPU_MC:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 30000, 500, 15000, 500, 4500, 50);
			break;
		case QUAD_CPU:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 26675, 500, 19995, 500, 2667, 50);
			break;
		case QUAD_CPU_MC:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 26675, 500, 19995, 500, 2667, 50);
			break;
		case QUAD_CPU_DUAL_SOCKET:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 36000, 1000, 6000, 700, 2600, 200);
			break;
		case QUAD_CPU_QUAD_SOCKET:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 39400, 600, 1500, 800, 2200, 150);
			break;
		case HEX_CPU_DUAL_SOCKET_SMT:
			count = topo.nr_cpus + 6;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 38000, 500, 3000, 1800, 2400, 80);
			break;
	}

	linsched_init(&topo);
	create_tasks(count, mask, 10, 20);
	linsched_run_sim(TEST_TICKS);

	validate_results((int*)expected_results);
}

/* Similar to the previous test, but require smaller overhead with
 * longer busy/sleep intervals */
void test_basic_bal2(int argc, char **argv) {
	int count, mask;
	struct linsched_topology topo;
	int type = parse_topology(argv[2]);
	int expected_results[MAX_RESULTS];

	topo = topo_db[type];

	switch (type) {
		case UNIPROCESSOR:
			count = topo.nr_cpus + 1;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 24000, 10, 24000, 10, 4920, 10);
			break;
		case DUAL_CPU:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 29600, 200, 15500, 300, 2650, 100);
			break;
		case DUAL_CPU_MC:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 28000, 1000, 17750, 1000, 3000, 200);
			break;
		case QUAD_CPU:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 24250, 100, 23640, 100, 1700, 50);
			break;
		case QUAD_CPU_MC:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 24250, 100, 23640, 100, 1700, 50);
			break;
		case QUAD_CPU_DUAL_SOCKET:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 37000, 700, 5000, 700, 600, 100);
			break;
		case QUAD_CPU_QUAD_SOCKET:
			count = topo.nr_cpus + 4;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 39000, 500, 2300, 1500, 380, 80);
			break;
		case HEX_CPU_DUAL_SOCKET_SMT:
			count = topo.nr_cpus + 6;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results,
				1, count, 39000, 600, 2000, 1000, 385, 65);
			break;
	}

	linsched_init(&topo);
	create_tasks(count, mask, 100, 200);
	linsched_run_sim(TEST_TICKS);

	validate_results((int*)expected_results);
}

/* Similar to basic_bal1, but make sure we can also have high-usage
 * cpu-locked tasks without making problems overall */
void test_bal1(int argc, char **argv) {
	int count, mask;
	struct linsched_topology topo;
	int type = parse_topology(argv[2]);
	int expected_results1[MAX_RESULTS];
	int expected_results2[MAX_RESULTS];

	topo = topo_db[type];

	switch (type) {
		case UNIPROCESSOR:
		case DUAL_CPU:
		case DUAL_CPU_MC:
			return;
		case QUAD_CPU:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results1,
				1, 6, 28500, 500, 17000, 1000, 1800, 100);
			initialize_expectations((int*)expected_results2,
				7, 8, 31000, 500, 21000, 500, 2300, 100);
			break;
		case QUAD_CPU_MC:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results1,
				1, 6, 28500, 1000, 17000, 1500, 1800, 100);
			initialize_expectations((int*)expected_results2,
				7, 8, 31500, 1000, 20000, 1000, 2200, 100);
			break;
		case QUAD_CPU_DUAL_SOCKET:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results1,
				1, 10, 36000, 1000, 6000, 500, 700, 100);
			initialize_expectations((int*)expected_results2,
				11, 12, 41000, 1000, 9000, 1000, 900, 100);
			break;
		case QUAD_CPU_QUAD_SOCKET:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results1,
				1, 18, 39000, 500, 2500, 2000, 350, 70);
			initialize_expectations((int*)expected_results2,
				19, 20, 45500, 1000, 3000, 1500, 500, 100);
			break;
		case HEX_CPU_DUAL_SOCKET_SMT:
			count = topo.nr_cpus + 2;
			mask = (1 << topo.nr_cpus) - 1;
			initialize_expectations((int*)expected_results1,
				1, 26, 39000, 1000, 2500, 2000, 350, 100);
			initialize_expectations((int*)expected_results2,
				27, 28, 45500, 2100, 3000, 2000, 450, 150);
			break;
	}

	linsched_init(&topo);
	create_tasks(count, mask, 100, 200);
	create_tasks(2, 0x3, 50, 200);
	linsched_run_sim(TEST_TICKS);

	validate_results((int*)expected_results1);
	validate_results((int*)expected_results2);
}

void test_list(int argc, char **argv);

struct test {
	char *name;
	void (*fn)(int, char**);
};

struct test tests[] = {
#define TEST(x) { #x, test_##x }
	TEST(trivial_bal),
	TEST(basic_bal1),
	TEST(basic_bal2),
	TEST(bal1),
	TEST(list),
};

void test_list(int argc, char **argv) {
	int i;
	for(i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		if(tests[i].fn != test_list) {
			puts(tests[i].name);
		}
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s <test name> <topo>|list\n", argv[0]);
	exit(1);
}

void test_main(int argc, char **argv) {
	int i;
	if(argc < 2) {
		usage(argv);
	}

	set_test_duration(TEST_TICKS);
	set_test_name(argv[1]);

	for(i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		if(!strcmp(argv[1], tests[i].name)) {
			tests[i].fn(argc, argv);
			return;
		}
	}
	usage(argv);
}
