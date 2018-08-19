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

#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/init_task.h>
#include <linux/fs.h>
#include <linux/mqueue.h>
#include <linux/personality.h>
#include <linux/fdtable.h>
#include <linux/fs_struct.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/desc.h>

/* STEIN */
#include "linux/wait.h"
/* STEIN */

void abort(void);

int __linsched_curr_cpu = 0;
struct group_info init_groups = { .usage = ATOMIC_INIT(2) };
DEFINE_PER_CPU(struct task_struct *, current_task) = &init_task;
u64 __jiffy_data jiffies = INITIAL_JIFFIES;

unsigned int cpu_khz; // needs correct initialization depending on core speeds
__cacheline_aligned DEFINE_RWLOCK(tasklist_lock);
struct task_struct *kthreadd_task;

struct nsproxy init_nsproxy;
struct user_struct root_user;
struct exec_domain default_exec_domain;
struct fs_struct init_fs;
struct files_struct init_files;
struct mm_struct init_mm;


long do_no_restart_syscall(struct restart_block *param) {return -EINTR;}
int security_task_getscheduler(struct task_struct *p,
                int policy, struct sched_param *lp) {return 0;}
int security_task_setscheduler(struct task_struct *p,
                int policy, struct sched_param *lp) {return 0;}
int security_task_setnice(struct task_struct *p, int nice) {return 0;}
void synchronize_sched(void) {}
unsigned long _raw_read_lock_irqsave(rwlock_t *lock) {return 0;}
void _raw_read_unlock_irqrestore(rwlock_t *lock, unsigned long flags) {}
void __raw_spin_lock_init(raw_spinlock_t *lock, const char *name,
                struct lock_class_key *key) {}
void __lockfunc _raw_spin_lock(raw_spinlock_t *lock) {}
void __lockfunc _raw_spin_unlock(raw_spinlock_t *lock) {}
void __lockfunc _raw_spin_unlock_irqrestore(raw_spinlock_t *lock, unsigned long flags) {}
int __lockfunc _raw_spin_trylock(raw_spinlock_t *lock) { return 1; }
unsigned long __lockfunc _raw_spin_lock_irqsave(raw_spinlock_t *lock) { return 0; }
void __lockfunc _raw_spin_lock_irq(raw_spinlock_t *lock) {}
void __lockfunc _raw_spin_unlock_irq(raw_spinlock_t *lock) {}
void __lockfunc _raw_read_lock(rwlock_t *lock) {}
void __lockfunc _raw_read_unlock(rwlock_t *lock) {}
int __printk_ratelimit(const char *func) {return 0;}
void __lockfunc _spin_lock(spinlock_t *lock) { }
void __lockfunc _spin_unlock(spinlock_t *lock) { }
void __lockfunc _spin_lock_irq(spinlock_t *lock) { }
void __lockfunc _spin_unlock_irq(spinlock_t *lock) { }
unsigned long __lockfunc _spin_lock_irqsave(spinlock_t *lock) { return 0; }
void __lockfunc _spin_unlock_irqrestore(spinlock_t *lock,
        unsigned long flags) { }
void __lockfunc _read_lock(rwlock_t *lock) { }
void __lockfunc _read_unlock(rwlock_t *lock) { }
int __lockfunc _spin_trylock(spinlock_t *lock) { return 1; }
int __lockfunc __reacquire_kernel_lock(void) { return 0; }
void __lockfunc __release_kernel_lock(void) { }
void mutex_lock_nested(struct mutex *lock, unsigned int subclass) {}
void __sched mutex_unlock(struct mutex *lock) { }
void rt_mutex_adjust_pi(struct task_struct *task) { }
int rt_mutex_getprio(struct task_struct *task) { return task->normal_prio; }
void add_preempt_count(int val) { }
void sub_preempt_count(int val) { }
void __sched preempt_schedule(void) {};
void enter_idle(void) {}
void __exit_idle(void) {}
void exit_idle(void) {}


asmlinkage int printk(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    return 0;
}

NORET_TYPE void panic(const char * fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    puts("");
    abort();
}

void dump_stack(void) { }
struct sighand_struct *lock_task_sighand(struct task_struct *tsk,
        unsigned long *flags) { return tsk->sighand; }
void linsched_change_cpu(int cpu) { __linsched_curr_cpu = cpu; }
unsigned int debug_smp_processor_id(void) { return __linsched_curr_cpu; }
int capable(int cap) { return 1; }
void fire_sched_out_preempt_notifiers(struct task_struct *curr,
        struct task_struct *next) { }
void fire_sched_in_preempt_notifiers(struct task_struct *curr) { }
struct thread_info *current_thread_info(void) {
    return (struct thread_info *) current->stack;
}

/*Kernel memory operations, subsitutes*/
void *kmalloc(size_t size, gfp_t flags) {
    void *res = malloc(size);
    if (res && (flags & __GFP_ZERO)) {
        memset(res, 0, size);
    }
    return res;
}

void kfree(const void *block) {
        free((void *)block);
}

/* These functions do not copy to and from user space anymore, so
 * they are just memory copy functions now.
 */
__must_check unsigned long
_copy_from_user(void *to, const void __user *from, unsigned n)
{
        memcpy(to, from, n);
        return 0;
}

__must_check unsigned long
_copy_to_user(void __user *to, const void *from, unsigned n)
{
        memcpy(to, from, n);
        return 0;
}

void copy_from_user_overflow(void)
{
}
/* find_task_by_pid_vpid: just a typecast is performed,
 * no actual mapping/hashing.
 */
struct task_struct *find_task_by_pid_vpid(pid_t nr)
{
        return (struct task_struct*)nr;
}

void user_disable_single_step(struct task_struct *child)
{
}

void warn_slowpath_null(const char *file, int line) {
    printf("WARNING: at %s:%d\n", file, line);
}

void warn_slowpath_fmt(const char *file, int line, const char *fmt, ...) {
    va_list list;
    warn_slowpath_null(file, line);
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
    puts("");
}

/* Added by STEIN */
void __init_waitqueue_head(wait_queue_head_t *q, struct lock_class_key *key)
{
	spin_lock_init(&q->lock);
	lockdep_set_class(&q->lock, key);
	INIT_LIST_HEAD(&q->task_list);
}
/* STEIN */
