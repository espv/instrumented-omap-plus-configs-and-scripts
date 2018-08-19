/* LinSched -- The Linux Scheduler Simulator
 * Copyright (C) 2008  John M. Calandrino
 * E-mail: jmc@cs.unc.edu
 *
 * This file contains Linux variables and functions that have been "defined
 * away" or exist here in a modified form to avoid including an entire Linux
 * source file that might otherwise lead to a "cascade" of dependency issues.
 * It also includes certain LinSched variables to which some Linux functions
 * and definitions now map.
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

#include <stdio.h>
#include <linux/tick.h>
/* linsched variables and functions */

struct task_struct *__linsched_tasks[LINSCHED_MAX_TASKS];
int curr_task_id = 1;
struct task_group *__linsched_groups[LINSCHED_MAX_GROUPS];
int curr_group_id = 0;

void linsched_init(struct linsched_topology *topo)
{
	linsched_init_cpus(topo);

	/* Change context to "boot" cpu and boot kernel. */
	linsched_change_cpu(0);

	/* initialize the root of the task group */
	__linsched_groups[curr_group_id] = &init_task_group;
	tg_set_id(&init_task_group, 0);
	curr_group_id++;

	sched_clock_stable = 1;

	start_kernel();

	linsched_init_hrtimer();
}

void linsched_default_callback(void) { }
void linsched_exit_callback(void) { do_exit(0); }
struct cfs_rq;
extern int cfs_rq_cpu(struct cfs_rq* cfs_rq);
void linsched_announce_callback(void)
{
	printf("CPU %d / t = %u: Task 0x%d scheduled, Runtime %llu\n", smp_processor_id(),
	       (unsigned int)jiffies, (unsigned int)current->id, ((struct task_struct*)current)->se.sum_exec_runtime);
}

void linsched_disable_migrations(void)
{
	int i;

	for (i = 0; i < curr_task_id; i++)
		set_cpus_allowed(__linsched_tasks[i],
				 cpumask_of_cpu(
					 task_cpu(__linsched_tasks[i])));
}

void linsched_enable_migrations(void)
{
	int i;

	for (i = 0; i < curr_task_id; i++)
		set_cpus_allowed(__linsched_tasks[i], CPU_MASK_ALL);
}

/* Force a migration of task to the dest_cpu.
 * If migr is set, allow migrations after the forced migration... otherwise,
 * do not allow them. (We need to disable migrations so that the forced
 * migration takes place correctly.)
 * Returns old cpu of task.
 */
int linsched_force_migration(struct task_struct *task, int dest_cpu, int migr)
{
	int old_cpu = task_cpu(task);

	linsched_disable_migrations();
	set_cpus_allowed(task, cpumask_of_cpu(dest_cpu));
	linsched_change_cpu(old_cpu);
	schedule();
	linsched_change_cpu(dest_cpu);
	schedule();
	if (migr)
		linsched_enable_migrations();

	return old_cpu;
}

/* Return the task in position task_id in the task array.
 * No error checking, so be careful!
 */
struct task_struct *linsched_get_task(int task_id)
{
	return __linsched_tasks[task_id];
}

struct task_group *linsched_get_group(int group_id)
{
	return __linsched_groups[group_id];
}

extern struct task_group init_task_group;
extern unsigned long tg_shares(struct task_group* tg);
extern void set_tg_shares(struct task_group* tg, unsigned long shares);
struct task_struct *__linsched_create_task(struct task_data* td)
{
	struct task_struct *newtask =
		(struct task_struct *)do_fork(0, 0, 0, 0, 0, 0, td);

	newtask->tg = &init_task_group;
	if(td->init_task) {
		td->init_task(newtask, td->data);
	}

	/* Allow task to run on any CPU. */
	set_cpus_allowed(newtask, CPU_MASK_ALL);

	linsched_check_resched();

	return newtask;
}

void linsched_check_resched(void) {
	while(need_resched()) {
		if(idle_cpu(smp_processor_id())) {
			tick_nohz_restart_sched_tick();
		}
		schedule();
	}
}

void __linsched_set_task_id(struct task_struct *p, int id)
{
	p->id = id;
}

/* Create a task group and return the group handle */
int linsched_create_task_group(int parent_group_id)
{
	struct task_group *parent, *child;
	int retval = curr_group_id;

	parent = linsched_get_group(parent_group_id);

	child = sched_create_group(parent);

	if (IS_ERR(child))
		return -1;

	__linsched_groups[curr_group_id] = child;
	tg_set_id(child, curr_group_id);
	curr_group_id++;

	return retval;
}

void linsched_set_task_group_shares(int group_id, unsigned long shares)
{
	struct task_group *tg = linsched_get_group(group_id);

	set_tg_shares(tg, shares);
}

int linsched_add_task_to_group(int task_id, int group_id)
{
	struct task_struct *p = linsched_get_task(task_id);

	p->tg = linsched_get_group(group_id);

	sched_move_task(p);

	return 0;
}

extern u64 task_exec_time(struct task_struct* p);
extern u64 group_exec_time(struct task_group* g);
void linsched_print_task_stats(void)
{
	int i;
	for (i = 1; i < curr_task_id; i++) {
		struct task_struct *task = __linsched_tasks[i];
		printf("Task id = %d, exec_time = %llu, run_delay = %llu, pcount = %lu\n", i,
		       task_exec_time(task),
		       task->sched_info.run_delay,
		       task->sched_info.pcount);
	}
}

/* We could have used the cpuacct cgroup for this, but a lot of the code
 * would have to be compiled out leading to a bunch of #ifdefs all over
 * the place. The simple function below is all we need.
 */
void linsched_print_group_stats(void)
{
	int i;
	for (i = 1; i < curr_group_id; i++) {
		printf("Group id = %d, exec_time = %llu\n", i,
				group_exec_time(__linsched_groups[i]));
	}
}

/* Create a normal task with the specified callback and
 * nice value of niceval, which determines its priority.
 */
int linsched_create_normal_task(struct task_data* td, int niceval)
{
	struct sched_param params = {};
	int retval = curr_task_id;

	/* If we cannot support any more tasks, return. */
	if (curr_task_id >= LINSCHED_MAX_TASKS)
		return -1;

	/* Create "normal" task and set its nice value. */
	__linsched_tasks[curr_task_id] = __linsched_create_task(td);

	__linsched_set_task_id(__linsched_tasks[curr_task_id], curr_task_id);

	params.sched_priority = 0;
	sched_setscheduler(__linsched_tasks[curr_task_id], SCHED_NORMAL,
			       &params);
	set_user_nice(__linsched_tasks[curr_task_id], niceval);

	/* Increment task id. */
	curr_task_id++;

	return retval;
}

/* Create a batch task with the specified callback and
 * nice value of niceval, which determines its priority.
 */
void linsched_create_batch_task(struct task_data* td, int niceval)
{
	struct sched_param params = {};

	/* If we cannot support any more tasks, return. */
	if (curr_task_id >= LINSCHED_MAX_TASKS)
		return;

	/* Create "batch" task and set its nice value. */
	__linsched_tasks[curr_task_id] = __linsched_create_task(td);
	__linsched_set_task_id(__linsched_tasks[curr_task_id], curr_task_id);
	params.sched_priority = 0;
	sched_setscheduler(__linsched_tasks[curr_task_id], SCHED_BATCH,
			       &params);
	set_user_nice(__linsched_tasks[curr_task_id], niceval);

	/* Increment task id. */
	curr_task_id++;
}

/* Create a FIFO real-time task with the specified callback and priority. */
void linsched_create_RTfifo_task(struct task_data* td, int prio)
{
	struct sched_param params = {};

	/* If we cannot support any more tasks, return. */
	if (curr_task_id >= LINSCHED_MAX_TASKS)
		return;

	/* Create FIFO real-time task and set its priority. */
	__linsched_tasks[curr_task_id] = __linsched_create_task(td);
	__linsched_set_task_id(__linsched_tasks[curr_task_id], curr_task_id);
	params.sched_priority = prio;
	sched_setscheduler(__linsched_tasks[curr_task_id], SCHED_FIFO,
			       &params);

	/* Increment task id. */
	curr_task_id++;
}

/* Create a RR real-time task with the specified callback and priority. */
void linsched_create_RTrr_task(struct task_data* td, int prio)
{
	struct sched_param params = {};

	/* If we cannot support any more tasks, return. */
	if (curr_task_id >= LINSCHED_MAX_TASKS)
		return;

	/* Create RR real-time task and set its priority. */
	__linsched_tasks[curr_task_id] = __linsched_create_task(td);
	__linsched_set_task_id(__linsched_tasks[curr_task_id], curr_task_id);
	params.sched_priority = prio;
	sched_setscheduler(__linsched_tasks[curr_task_id], SCHED_RR, &params);

	/* Increment task id. */
	curr_task_id++;
}

void linsched_yield(void)
{
	/* If the current task is not the idle task, yield. */
	if (current != idle_task(smp_processor_id()))
		yield();
}

u64 rq_clock_delta(struct task_struct *p);
/* The standard task_data for a task which runs the busy/sleep setup */
static enum hrtimer_restart wake_task(struct hrtimer *timer)
{
	struct sleep_run_data *d = container_of(timer, struct sleep_run_data, timer);
	wake_up_process(d->p);
	return HRTIMER_NORESTART;
}

static void sleep_run_init(struct task_struct *p, void *data) {
	struct sleep_run_data *d = data;
	d->p = p;
	d->last_start = 0;
	hrtimer_init(&d->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	d->timer.function = wake_task;
}

static void sleep_run_handle(struct task_struct *p, void *data) {
	struct sleep_run_data *d = data;
	u64 cur = task_exec_time(p);
	if(cur - d->last_start >= d->busy * NSEC_PER_MSEC) {
		d->last_start = cur;
		if(d->sleep) {
			p->state = TASK_INTERRUPTIBLE;
			hrtimer_set_expires(&d->timer, ns_to_ktime(d->sleep * NSEC_PER_MSEC));
			hrtimer_start_expires(&d->timer, HRTIMER_MODE_REL);
			schedule();
		}
	}
}

struct task_data *linsched_create_sleep_run(int sleep, int busy) {
	struct task_data *td = malloc(sizeof(struct task_data));
	struct sleep_run_data *d =  malloc(sizeof(struct sleep_run_data));
	d->sleep = sleep;
	d->busy = busy;
	td->data = d;
	td->init_task = sleep_run_init;
	td->handle_task = sleep_run_handle;
	return td;
}
