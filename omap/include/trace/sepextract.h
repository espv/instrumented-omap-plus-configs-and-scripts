#undef TRACE_SYSTEM
#define TRACE_SYSTEM sepextract

#if !defined(__TP_SEPEXTRACT_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __TP_SEPEXTRACT_TRACE_H

#include <linux/tracepoint.h> 
#include <linux/semaphore.h>
#include <trace/sepextract-structures.h>

// #undef DEFINE_TRACE_NOARGS
#if 0
/* Based on linux/tracepoint.h DEFINE_TRACE macro */
#define DEFINE_TRACE_NOARGS(name)         \
  static inline void trace_##name(void)       \
  {               \
		static const char __tpstrtab_##name[]			\
		__attribute__((section("__tracepoints_strings")))	\
		= #name ":" "void" ;     					\
		static struct tracepoint __tracepoint_##name		\
		__attribute__((section("__tracepoints"), aligned(8))) =	\
		{ __tpstrtab_##name, 0, NULL };				\
		if (unlikely(__tracepoint_##name.state))		\
    	do {								\
		    void **it_func;						\
									\
    		rcu_read_lock_sched();					\
	    	it_func = rcu_dereference((&__tracepoint_##name)->funcs);			\
	    	if (it_func) {						\
		  	do {						\
			  	((void(*)(void))(*it_func))();	\
  			} while (*(++it_func));				\
  		}							\
  		rcu_read_unlock_sched();				\
  	} while (0); \
	}							\
	static inline int register_trace_##name(void (*probe)(void))	\
	{								\
		return tracepoint_probe_register(#name ":" "void",	\
			(void *)probe);					\
	}								\
	static inline void unregister_trace_##name(void (*probe)(void))\
	{								\
		tracepoint_probe_unregister(#name ":" "void",		\
			(void *)probe);					\
	} \

#endif

/* The following function is used to
   obtain the current address in order to
   uniquely identify queues.

   DEPRECATED: Currently obtained per
   event */
void *sepext_get_address(void);

/* instrumented protocols */

/*
 * PIOTR
 *  Kernel 2.6.28 does not support trace declarations
 */

DECLARE_TRACE(sepext_srventry,
	      TP_PROTO(void *address), 
	      TP_ARGS(address));

DECLARE_TRACE(sepext_srvexit,
	      TP_PROTO(void *address),
	      TP_ARGS(address));

DECLARE_TRACE(sepext_ctxswitch,
        TP_PROTO(struct task_struct *prev, struct task_struct *next),
        TP_ARGS(prev, next));

/* kernel/irq/handle.c: */
DECLARE_TRACE(sepext_hirqstart,
	TP_PROTO(unsigned int irq),
		TP_ARGS(irq));

DECLARE_TRACE(sepext_hirqstop,
	TP_PROTO(unsigned int irq),
		TP_ARGS(irq));

DECLARE_TRACE(sepext_peustart,
              TP_PROTO(void *address),
              TP_ARGS(address));

DECLARE_TRACE(sepext_peustop,
              TP_PROTO(void *address),
              TP_ARGS(address));

DECLARE_TRACE(sepext_pktqueue,
	      TP_PROTO(void *pktdata, unsigned int queue, void *head, enum sem_queue_operation operation),
	      TP_ARGS(pktdata, queue, head, operation));

DECLARE_TRACE(sepext_srvqueue,
	      TP_PROTO(void *address, unsigned int queue, void *head, enum sem_queue_operation operation),
	      TP_ARGS(address, queue, head, operation));

#if 0
DECLARE_TRACE(sepext_srvenq,
	      TP_PROTO(void *address),
	      TP_ARGS(address));

DECLARE_TRACE(sepext_srvdeq,
	      TP_PROTO(void *address),
	      TP_ARGS(address));
#endif

DECLARE_TRACE(sepext_loopstart,
        TP_PROTO(int maxIterations, short perQueue, unsigned int fromQueue, unsigned int toQueue),
        TP_ARGS(maxIterations, perQueue, fromQueue, toQueue));


// DECLARE_TRACE(sepext_looprestart)
DECLARE_TRACE(sepext_looprestart,
        TP_PROTO(void *dummy),
        TP_ARGS(dummy));

DECLARE_TRACE(sepext_loopstop,
        TP_PROTO(void *dummy),
        TP_ARGS(dummy));

DECLARE_TRACE(sepext_migrate,
	      TP_PROTO(int newCpu, int pid),
	      TP_ARGS(newCpu, pid));

DECLARE_TRACE(sepext_queuecondition,
	      TP_PROTO(enum sem_queue_condition condition, unsigned int from, unsigned int to),
	      TP_ARGS(condition, from, to));

DECLARE_TRACE(sepext_statecondition,
	     TP_PROTO(enum sem_state_id condition, unsigned long value, unsigned int readwrite, unsigned int global),
	     TP_ARGS(condition, value, readwrite, global));

DECLARE_TRACE(sepext_semaphore_up,
	      TP_PROTO(void *location, struct semaphore *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_semaphore_down,
	      TP_PROTO(void *location, struct semaphore *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_wakeup,
	      TP_PROTO(void *location, struct task_struct *p),
	      TP_ARGS(location, p));

DECLARE_TRACE(sepext_dequeuerq,
	      TP_PROTO(void *location, struct task_struct *p, int flags),
	      TP_ARGS(location, p, flags));

DECLARE_TRACE(sepext_wait_for_completion,
	      TP_PROTO(void *location, struct completion *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_wait_for_completion_timeout,
	      TP_PROTO(void *location, struct completion *x, unsigned long timeout, enum sem_synch_scope scope),
	      TP_ARGS(location, x, timeout, scope));

DECLARE_TRACE(sepext_wait_for_completion_interruptible,
	      TP_PROTO(void *location, struct completion *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_wait_for_completion_interruptible_timeout,
	      TP_PROTO(void *location, struct completion *x, unsigned long timeout, enum sem_synch_scope scope),
	      TP_ARGS(location, x, timeout, scope));

DECLARE_TRACE(sepext_wait_for_completion_killable,
	      TP_PROTO(void *location, struct completion *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_wait_for_completion_killable_timeout,
	      TP_PROTO(void *location, struct completion *x, unsigned long timeout, enum sem_synch_scope scope),
	      TP_ARGS(location, x, timeout, scope));

DECLARE_TRACE(sepext_try_wait_for_completion,
	      TP_PROTO(void *location, struct completion *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_complete,
	      TP_PROTO(void *location, struct completion *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_complete_all,
	      TP_PROTO(void *location, struct completion *x, enum sem_synch_scope scope),
	      TP_ARGS(location, x, scope));

DECLARE_TRACE(sepext_tempsynch,
	     TP_PROTO(void *location, enum sem_synch_type type, void *variable, enum sem_synch_scope scope),
	     TP_ARGS(location, type, variable, scope));

/* kernel/softirq.c */
DECLARE_TRACE(sepext_softirqraise,
        TP_PROTO(unsigned int sirq),
		TP_ARGS(sirq));

DECLARE_TRACE(sepext_softirqstart,
        TP_PROTO(unsigned int sirq),
        TP_ARGS(sirq));

DECLARE_TRACE(sepext_softirqstop,
        TP_PROTO(unsigned int sirq),
		TP_ARGS(sirq));

DECLARE_TRACE(sepext_pktextract,
	      TP_PROTO(void* value, unsigned int length, unsigned int global),
	      TP_ARGS(value, length, global));


#if 0

DECLARE_TRACE(sepext_statequeue,
	     TP_PROTO(unsigned int queue, unsigned int value, enum sem_queue_operation operation, unsigned int scope),
	     TP_ARGS(queue, value, operation, scope));

#if 0 // OYSTEDAL
#endif

DECLARE_TRACE(sepext_pktextr,
	      TP_PROTO(void *pkt, unsigned int location, void *queue),
	      TP_ARGS(pkt, location, queue));

DECLARE_TRACE(sepext_pktstart,
	      TP_PROTO(void *address),
	      TP_ARGS(address));

DECLARE_TRACE(sepext_pktstop,
	      TP_PROTO(void *address),
	      TP_ARGS(address));

DECLARE_TRACE(sepext_pktenq,
	      TP_PROTO(void *address),
	      TP_ARGS(address));

DECLARE_TRACE(sepext_pktdeq,
	      TP_PROTO(void *address),
	      TP_ARGS(address));

DECLARE_TRACE(sepext_threadcondition,
	      TP_PROTO(enum sem_thread_condition condition, unsigned long pid),
	      TP_ARGS(condition, pid));

DECLARE_TRACE_NOARGS(sepext_conditionend);
// DECLARE
/* kernel/sched.c: */ 
DECLARE_TRACE(sepext_enqueuerq,
	      TP_PROTO(void *location, struct task_struct *p, int flags),
	      TP_ARGS(location, p, flags));

DECLARE_TRACE(sepext_taskletstart,
	TP_PROTO(unsigned long tasklet),
		TP_ARGS(tasklet));

DECLARE_TRACE(sepext_taskletstop,
	TP_PROTO(unsigned long tasklet),
		TP_ARGS(tasklet));

/* Stacktracing */
DECLARE_TRACE_NOARGS(sepext_stacktrace);

/* Only for DEBUG purposes */
DECLARE_TRACE(sepext_debug,
	TP_PROTO(unsigned long d),
	TP_ARGS(d));

#endif

#endif

/* this part must be outside the header guard */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sepextract

#include <trace/define_trace.h>

