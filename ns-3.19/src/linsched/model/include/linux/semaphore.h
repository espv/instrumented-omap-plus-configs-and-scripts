/*
 * Copyright (c) 2008 Intel Corporation
 * Author: Matthew Wilcox <willy@linux.intel.com>
 *
 * Distributed under the terms of the GNU GPL, version 2
 *
 * Please see kernel/semaphore.c for documentation of these functions
 *
 *
 * STEIN: added linsched ifndefs
 */
#ifndef __LINUX_SEMAPHORE_H
#define __LINUX_SEMAPHORE_H

#include <linux/list.h>

#ifndef __LINSCHED__
#include <linux/spinlock.h>
#endif

/* Please don't access any members of this structure directly */
struct semaphore {
#ifndef __LINSCHED__
	spinlock_t		lock;
#endif
	unsigned int		count;
	struct list_head	wait_list;
};

#ifndef __LINSCHED__
#define __SEMAPHORE_INITIALIZER(name, n)				\
{

	.lock		= __SPIN_LOCK_UNLOCKED((name).lock),		\
	.count		= n,						\
	.wait_list	= LIST_HEAD_INIT((name).wait_list),		\
}
#endif

#ifdef __LINSCHED__
#define __SEMAPHORE_INITIALIZER(name, n)				\
{                                                                       \
	.count		= n,						\
	.wait_list	= LIST_HEAD_INIT((name).wait_list),		\
}
#endif

#define DECLARE_MUTEX(name)	\
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)

static inline void sema_init(struct semaphore *sem, int val)
{
#ifndef __LINSCHED__
	static struct lock_class_key __key;
#endif
	*sem = (struct semaphore) __SEMAPHORE_INITIALIZER(*sem, val);
#ifndef __LINSCHED__
	lockdep_init_map(&sem->lock.dep_map, "semaphore->lock", &__key, 0);
#endif
}

#define init_MUTEX(sem)		sema_init(sem, 1)
#define init_MUTEX_LOCKED(sem)	sema_init(sem, 0)

extern void down(struct semaphore *sem);
extern int __must_check down_interruptible(struct semaphore *sem);
extern int __must_check down_killable(struct semaphore *sem);
extern int __must_check down_trylock(struct semaphore *sem);
extern int __must_check down_timeout(struct semaphore *sem, long jiffies);
extern void up(struct semaphore *sem);

#endif /* __LINUX_SEMAPHORE_H */
