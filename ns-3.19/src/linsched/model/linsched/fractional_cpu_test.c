/* Fractional CPU test for the Linux Scheduler Simulator
 *
 * A fractional CPU test that starts of two groups of tasks (a protagonist
 * and an antagonist) with varying CPU share allocations. The net effect
 * of the CPU alloction by the scheduler is measured by the amount of run
 * time alloted to each group at the end of the simulation.
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

#include "linsched.h"
#include "linux_linsched.h"
#include <strings.h>
#include <getopt.h>
#include <stdio.h>
#include <asm/current.h>

#include "cosim.h"

struct task_data dummy_td_fractional;

// Used explicitly here to configure the task group
extern void set_tg_shares(struct task_group* tg, unsigned long shares);

int fractional(void)
{

  // Initialize linux data structures
  linsched_init(UNIPROCESSOR);
  
  // Create one group into which we add threads
  struct task_group *group;
  group = sched_create_group(&init_task_group);
  set_tg_shares(group, 1024);
  
  // Create thread 1
/*   struct sched_param params = {};	 */
/*   struct task_struct *task1 = (struct task_struct *)do_fork(0, 0, 0, 0, 0, 0, &dummy_td_fractional); */
/*   task1->tg = &init_task_group; */
/*   set_cpus_allowed(task1, CPU_MASK_ALL);	 */
/*   linsched_check_resched(); */
/*   sched_setscheduler(task1, SCHED_NORMAL, &params); */
/*   set_user_nice(task1, 0); */
/*   task1->tg = group; */
/*   sched_move_task(task1); */
  
/*   // Create thread 2 */
/*   struct task_struct *task2 = (struct task_struct *)do_fork(0, 0, 0, 0, 0, 0, &dummy_td_fractional); */
/*   task2->tg = &init_task_group; */
/*   set_cpus_allowed(task2, CPU_MASK_ALL);	 */
/*   linsched_check_resched(); */
/*   sched_setscheduler(task2, SCHED_NORMAL, &params); */
/*   set_user_nice(task2, 0); */
/*   task2->tg = group; */
/*   sched_move_task(task2); */
  
/*   // Create thread 3 */
/*   struct task_struct *task3 = (struct task_struct *)do_fork(0, 0, 0, 0, 0, 0, &dummy_td_fractional); */
/*   task3->tg = &init_task_group; */
/*   set_cpus_allowed(task3, CPU_MASK_ALL);	 */
/*   linsched_check_resched(); */
/*   sched_setscheduler(task3, SCHED_NORMAL, &params); */
/*   set_user_nice(task3, 0); */
/*   task3->tg = group; */
/*   sched_move_task(task3); */

/* 	// Create another task */
/* 	struct task_struct *task2 = (struct task_struct *)do_fork(0, 0, 0, 0, 0, 0, &dummy_td_fractional); */
/* 	task2->tg = &group; */
/* 	sched_move_task(task2); */
/* 	set_cpus_allowed(task2, CPU_MASK_ALL); */
/* 	sched_setscheduler(task2, SCHED_NORMAL, &params); */

/* 	return 0; */


	/////////////////////////////
	// (Almost) original version:
	/////////////////////////////
/*         int t1, g1, i, c; */
/*         unsigned long pthreads = 5, athreads = 5; */
/*         unsigned long pshares = 1024,ashares = 512; */
/*         unsigned long busy = 1, sleep = 1; */

        /* Initialize linsched. */
/*         linsched_init(UNIPROCESSOR); */

/*         g1 = linsched_create_task_group(0); */
/*         linsched_set_task_group_shares(g1, pshares); */

/*         /\* Create protagonist *\/ */
/*         for (i = 0; i < pthreads; i++) { */
/* 	  t1 = linsched_create_normal_task(linsched_create_sleep_run(sleep, busy), 0); */
/* 	  linsched_add_task_to_group(t1, g1); */
/*         } */

/*         /\* Create antagonist *\/ */
/*         g1 = linsched_create_task_group(0); */
/*         linsched_set_task_group_shares(g1, ashares); */
/*         for (i = 0; i < athreads; i++) { */
/* 	  t1 = linsched_create_normal_task(linsched_create_sleep_run(sleep, busy), 0); */
/* 	  linsched_add_task_to_group(t1, g1); */
/*         } */

/*         return 0; */
}
