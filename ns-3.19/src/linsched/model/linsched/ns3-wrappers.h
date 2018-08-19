#ifndef NS3_WRAPPERS_H
#define NS3_WRAPPERS_H

void set_time(unsigned long nanoseconds);
int get_cr_pid();
void initialize_linsched();
void* create_thread(int priority, int *newPid);
void *new_semaphore(int value);
void *new_completion();
void semaphore_up(void *semaphore);
void semaphore_down(void *semaphore);
void completion_wait(void *completion);
void completion_complete(void *completion);
void exit_thread(void);
void goto_sleep(void);
void awake(void *thread);

#endif
