#include "linsched.h"
#include "linux_linsched.h"
#include <strings.h>
#include <getopt.h>
#include <stdio.h>
#include <asm/current.h>
#include <linux/semaphore.h>
#include <linux/completion.h>

void set_time(unsigned long nanoseconds) {
	current_time = nanoseconds;
}

int get_cr_pid()
{
  struct task_struct *cr = get_current();
  return cr->pid;
}

struct task_data dummy_td;
struct sched_param params = {};	

// Used explicitly here to configure the task group
extern void set_tg_shares(struct task_group* tg, unsigned long shares);
struct task_group *group;

void initialize_linsched() {
  //linsched_init(UNIPROCESSOR);
  // OYSTEDAL: 
  struct linsched_topology topo = TOPO_DUAL_CPU_MC;
  linsched_init(&topo);

  group = sched_create_group(&init_task_group);
  set_tg_shares(group, 1024);

#if 0
  // OYSTEDAL: create test thread
  // Doesn't work
  struct task_struct *task = do_fork(0, 0, 0, 0, 0, 0, &dummy_td);
  task->tg = &init_task_group;
  cpumask_t mask;
  cpumask_set_cpu(1, &mask);
  set_cpus_allowed(task, mask);
  linsched_check_resched();
  sched_setscheduler(task, SCHED_NORMAL, &params);
  set_user_nice(task, 0);
  task->tg = group;
  sched_move_task(task);
#endif
}

void* create_thread(int priority, int *newPid) {
  struct task_struct *task = do_fork(0, 0, 0, 0, 0, 0, &dummy_td);
  task->tg = &init_task_group;
  // set_cpus_allowed(task, CPU_MASK_ALL);	
  // OYSTEDAL: only use cpu0 for now
  set_cpus_allowed(task, CPU_MASK_CPU0);
  linsched_check_resched();
  sched_setscheduler(task, SCHED_NORMAL, &params);
  set_user_nice(task, priority);
  task->tg = group;
  sched_move_task(task);

  *newPid = task->pid;
  return (void *) task;
}

void exit_thread(void)
{
  do_exit(0);
}

void* new_semaphore(int value)
{
  void *alloced = malloc(sizeof(struct semaphore));

  struct semaphore *sem = (struct semaphore *) alloced;
  sema_init(sem, value);

  return alloced;
}

void* new_completion()
{
  void *alloced = malloc(sizeof(struct completion));

  struct completion *compl = (struct completion *) alloced;
  init_completion(compl);

  return alloced;
}

void semaphore_up(void *semaphore)
{
  struct semaphore *sem = (struct semaphore *) semaphore;
//  printf(" UP ");
  up(sem);
  linsched_check_resched();
}

void semaphore_down(void *semaphore)
{
  struct semaphore *sem = (struct semaphore *) semaphore;
//  printf(" DOWN ");
  down(sem);
  linsched_check_resched();
}

void completion_wait(void *completion)
{
  struct completion *compl = (struct completion *) completion;
//  printf(" WAIT ");
  wait_for_completion(compl);
  linsched_check_resched();
}

void completion_complete(void *completion)
{
  struct completion *compl = (struct completion *) completion;
//  printf(" COMPLETE ");
  complete(compl);
  linsched_check_resched();
}

void goto_sleep(void)
{
	set_current_state(TASK_INTERRUPTIBLE);
	schedule();
	linsched_check_resched();
}

void awake(void* threadToAwake)
{
	struct task_struct* toAwake = threadToAwake;
//	wake_up_process(toAwake);
	wake_up_state(toAwake, TASK_INTERRUPTIBLE);
	linsched_check_resched();
}
