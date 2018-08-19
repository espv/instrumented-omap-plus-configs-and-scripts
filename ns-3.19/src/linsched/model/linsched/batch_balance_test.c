/* Test for fixes to b/2236106 for the Linux Scheduler Simulator
 *
 * Make sure that /batch tasks get load balanced properly; when they
 * don't you get b/2236106. Define "properly" as within 80% of
 * perfectly fair, and 95% total utilization.
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
#include <strings.h>
#include <stdio.h>

#define INTERVAL_MSEC 60000
#define ONE_CORE_SHARES (64000/16)
#define NBATCH 16
#define TOTAL_CPUTIME ((INTERVAL_MSEC * (u64)NSEC_PER_MSEC) * 16)

#define NLS 3

void test_main(int argc, char **argv) {
    struct linsched_topology topo = TOPO_QUAD_CPU_QUAD_SOCKET;
    int batch, sys, i;
    u64 total = 0;
    linsched_init(&topo);

    batch = linsched_create_task_group(0);
    sched_group_set_shares(__linsched_groups[batch], 2);

    sys = linsched_create_task_group(0);
    sched_group_set_shares(__linsched_groups[sys], ONE_CORE_SHARES);


    for(i = 0; i < NLS; i++) {
        int task = create_task(0xffff, 0, 100);
        linsched_add_task_to_group(task, sys);
    }

    /* we create 40 batch task groups, each with .4 cores worth of
     * shares and 1 100% running task in each group */
    for(i = 0; i < NBATCH; i++) {
        int task = create_task(0xffff, 0, 100);
        linsched_add_task_to_group(task, batch);
        /* force them all to get balanced off /after/ being in the batch group */
        set_cpus_allowed(__linsched_tasks[task], *cpumask_of(0));
        set_cpus_allowed(__linsched_tasks[task], *cpu_online_mask);
    }

    linsched_run_sim(INTERVAL_MSEC);

    for(i = 0; i < NBATCH + NLS; i++) {
        total += __linsched_tasks[i+1]->se.sum_exec_runtime;
    }

    expect_failure();
    /* this test isn't quite so much of a requirement - utilization is more important than fairness here */
    expect_tasks_all(1, NBATCH, TOTAL_CPUTIME / NBATCH * 8/10, TOTAL_CPUTIME / NBATCH * 2/10,
                     0, ~0ULL, 0, ~0);

    expect_failure();
    expect(total >= TOTAL_CPUTIME * 95/100);
}
