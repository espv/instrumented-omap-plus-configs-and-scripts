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
#include "cosim.h"

static int linsched_hrt_set_next_event(unsigned long evt,
                                       struct clock_event_device *d);
static void linsched_hrt_set_mode(enum clock_event_mode mode,
                                  struct clock_event_device *d);
static void linsched_hrt_broadcast(const struct cpumask *mask);
static cycle_t linsched_source_read(struct clocksource *cs);

u64 current_time;
/* STEIN */
#ifdef __NS3_COSIM__
//u64 next_linsched_event;
void *data;
extern unsigned long next_schedsim_event;
extern unsigned long next_netsim_event;
extern int numevs;
//u64 next_ns3_event;
#endif

static u64 next_event[NR_CPUS];
static struct clock_event_device linsched_hrt[NR_CPUS];

static struct clock_event_device linsched_hrt_base = {
    .name = "linsched-events",
    .features = CLOCK_EVT_FEAT_ONESHOT,
    .max_delta_ns = 1000000000L,
    .min_delta_ns = 5000,
    .mult = 1,
    .shift = 0,
    .rating = 100,
    .irq = -1,
    .cpumask = NULL,
    .set_next_event = linsched_hrt_set_next_event,
    .set_mode = linsched_hrt_set_mode,
    .broadcast = linsched_hrt_broadcast
};

/* We need a clocksource other than jiffies in order to run
 * HIGH_RES_TIMERS, so replace the default. jiffies will then be
 * updated by kernel/time/tick-common.c and
 * kernel/time/tick-sched.c */
static struct clocksource linsched_hrt_source = {
    .name = "linsched-clocksource",
    .rating = 100,
    .mask = (cycle_t)-1,
    .mult = 1,
    .shift = 0,
    .flags = CLOCK_SOURCE_VALID_FOR_HRES | CLOCK_SOURCE_IS_CONTINUOUS,
    .read = linsched_source_read,
};

extern int __cpuinit hrtimer_cpu_notify(struct notifier_block *self, unsigned long action, void *hcpu);

void linsched_init_hrtimer(void) {
    int i;

    for(i = 0; i < nr_cpu_ids; i++) {
        struct clock_event_device *dev = &linsched_hrt[i];
        memcpy(dev, &linsched_hrt_base, sizeof(struct clock_event_device));
        dev->cpumask = cpumask_of(i);

        next_event[i] = KTIME_MAX;

        linsched_change_cpu(i);
        clockevents_register_device(dev);
        /* fake an activation message for this cpu, except cpu 0,
         * which is initialized by start_kernel() */
        if(i) {
            hrtimer_cpu_notify(NULL, CPU_UP_PREPARE, (void*) i);
        }
    }
    /* Normally this would be called when looking through
     * clocksources, but we're not using the entire clocksource
     * infrastructure at the moment */
    tick_clock_notify();
    for(i = 0; i < nr_cpu_ids; i++) {
        linsched_change_cpu(i);
        hrtimer_run_pending();
    }
    linsched_change_cpu(0);
}

void linsched_run_sim(int sim_ticks)
{
    /* Run a simulation for some number of ticks. Each tick,
     * scheduling and load balancing decisions are made. Obviously, we
     * could create tasks, change priorities, etc., at certain ticks
     * if we desired, rather than just running a simple simulation.
     * (Tasks can also be removed by having them exit.)
     */

    u64 initial_jiffies = jiffies;
    cpumask_t runnable;

    while(current_time < KTIME_MAX && jiffies < initial_jiffies + sim_ticks) {
        int i;
        u64 evt = KTIME_MAX;
        /* find the next event */
        for(i = 0; i < nr_cpu_ids; i++) {
            if(next_event[i] < evt) {
                evt = next_event[i];
                cpumask_clear(&runnable);
            }
            if(next_event[i] == evt) {
                cpumask_set_cpu(i, &runnable);
            }
        }
        current_time = evt;
        int active_cpu = 0;

        /* It might be useful to randomize the ordering here, although
         * it should be rare that there will actually be two active
         * cpus at once as the tick code offsets the main scheduler
         * ticks for each cpu */
        for_each_cpu(active_cpu, &runnable) {
            next_event[active_cpu] = KTIME_MAX;
            linsched_change_cpu(active_cpu);
	    linsched_hrt[active_cpu].event_handler(&linsched_hrt[active_cpu]);
            if (idle_cpu(smp_processor_id()) && !need_resched())
                 tick_nohz_stop_sched_tick(1);

            linsched_check_resched();

            struct task_struct *old;
            do {
                old = current;
                if (current->td && current->td->handle_task) {
                    current->td->handle_task(current, current->td->data);
                }
                /* we keep calling handle_task so that tasks can have
                 * < 1ms runtimes if they want */
            } while(current != old);

            /* First time executing a task? Do not need to
             * call schedule_tail, since we are not actually
             * performing a "real" context switch.
             */
        }
    }
}

struct clocksource * __init __weak clocksource_default_clock(void)
{
    return &linsched_hrt_source;
}

#ifdef __NS3_COSIM__
/*
   TODO: Add node, and use set-node() to set node-structure from
   which get_cpu_var, etc. retrieves variables.

   NOTE: Currently only supports unicore CPUs, by means of never
   changing to simulating any other CPU
*/
#if 0
void run_event_handler() {
#ifdef __NS3_COSIM_DEBUG__
    printf("%s(%p)\n",  __FUNCTION__, data);
#endif

    cpumask_t runnable;
    struct clock_event_device *dev = (struct clock_event_device *) data;
    current_time = next_schedsim_event;

    while(current_time <= next_netsim_event) {
        printf("LS: %lu\n", current_time);
        printf("NS: %lu\n", next_netsim_event);

        struct task_struct *old, *new;

        int i;
        u64 evt = KTIME_MAX;
        /* find the next event */
        for(i = 0; i < 1; i++) {
            if(next_event[i] < evt) {
                evt = next_event[i];
                cpumask_clear(&runnable);
            }
            if(next_event[i] == evt) {
                cpumask_set_cpu(i, &runnable);
            }
        }

        next_schedsim_event = evt;

        int active_cpu = 0;
        u64 temp = next_netsim_event;
        runnable = CPU_MASK_CPU0;

        for_each_cpu(active_cpu, &runnable) {
            // printf("CPU%d is runnable\n", active_cpu);
            old = get_current();
            linsched_hrt[active_cpu].event_handler(&linsched_hrt[active_cpu]);
            linsched_check_resched();
            new = get_current();

            /* Check if the task changed. If so, inform NetSim, and update next_netsim_event */
            if(new != old) {
                //  printf("%d -> %d", old->pid, new->pid);
                next_netsim_event = ns3_preempt(new->pid);
                printf("next netsim event: %lu\n", temp);
            }
            if (idle_cpu(smp_processor_id()) && !need_resched())
                tick_nohz_stop_sched_tick(1);
        }

        current_time = next_schedsim_event;
    }
}
#else
void run_event_handler() {
  #ifdef __NS3_COSIM_DEBUG__
  printf("%s(%p)\n",  __FUNCTION__, data);
  #endif

  struct clock_event_device *dev = (struct clock_event_device *) data;
  current_time = next_schedsim_event;

  while(current_time <= next_netsim_event) {
	  struct task_struct *old, *new;
	  numevs++;

	  old = get_current();
	  dev->event_handler(dev);
	  linsched_check_resched();
	  new = get_current();

	  /* Check if the task changed. If so, inform NetSim, and update next_netsim_event */
	  if(new != old) {
		//  printf("%d -> %d", old->pid, new->pid);
		  next_netsim_event = ns3_preempt(new->pid);
	  }
	  if (idle_cpu(smp_processor_id()) && !need_resched())
		  tick_nohz_stop_sched_tick(1);

	  current_time = next_schedsim_event;
  }
//	  struct task_struct *old;
//	  do {
//		  old = current;
//		  if (current->td && current->td->handle_task) {
//			  current->td->handle_task(current, current->td->data);
//		  }
//		  /* we keep calling handle_task so that tasks can have
//		   * < 1ms runtimes if they want */
//	  } while(current != old);


}
#endif
#endif

static int linsched_hrt_set_next_event(unsigned long evt,
                                       struct clock_event_device *d) {
  #ifdef __NS3_COSIM__
//  schedule_ns3_event(evt, run_event_handler, (void *) d);
  next_schedsim_event = current_time + evt;

  // next_event[d-linsched_hrt] = current_time + evt;
  // printf("CPU%d - next event: %lu\n", smp_processor_id(), current_time + evt);
  data = d;
  return 0;
  #else
  printf("test\n");
  next_event[d - linsched_hrt] = ktime_to_ns(ktime_add_safe(ns_to_ktime(current_time), ns_to_ktime(evt)));
  return 0;
  #endif
}
static void linsched_hrt_set_mode(enum clock_event_mode mode,
                                  struct clock_event_device *d) {}
static void linsched_hrt_broadcast(const struct cpumask *mask) {}

static cycle_t linsched_source_read(struct clocksource *cs) {
    return current_time;
}

u64 sched_clock(void) {
    return current_time;
}