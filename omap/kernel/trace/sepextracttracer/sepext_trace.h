#ifndef __SEPEXT_TRACE_H
#define __SEPEXT_TRACE_H

#ifndef USNL

#include <linux/kprobes.h>

#endif

#define DEBUG 0
#define NETLINK_NICTEST  17
#define END_OF_DATA 1000001
#define START_TRACING 1000002
#define STOP_TRACING 1000003
#define SYNC_COMPLETE 1000004
#define WAKE_UP_USNL 1000005
#define REQUEST_ENTRY 1000006
#define WARMUPTIME 100

/* DEPRECATED: we only need packet extracts on en/de-queue operations - in fact only
   de-queue operations, as we assume that direct service calls regard the same
   packet as that handled by the caller. */
#define PKTEXTR(pktid) trace_entry(0, PKTEXTRACT, 0, 0, pktid, (void *) __FUNCTION__)
#define PKTEXTRN(pktid) trace_entry(0, PKTEXTRACTN, 0, 0, pktid, (void *) __FUNCTION__)

/* Types for the different kinds of trace entries */
enum sepentry_type {
  __TYPE_FIRST = 0,

  CTXSWITCH,
  SERVICEENTRY,
  SERVICEEXIT,
  PEUSTART,
  PEUSTOP,
  PKTEXTRACT,
  PKTEXTRACTN,
  PKTSTART,
  PKTSTOP,
  PKTENQ,
  PKTDEQ,
  LOOPSTART,
  LOOPRESTART,
  LOOPSTOP,
  QUEUECOND,
  THREADCOND,
  STATECOND,
  CONDEND,
  PKTQUEUE,
  PKTQUEUEN,
  SRVQUEUE,
  STATEQUEUE,

  TTWAKEUP,
  WAKEUP,
  SIRQRAISE,
  SLEEP,
  HIRQSTART,
  HIRQSTOP,
  SOFTIRQSTART,
  SOFTIRQSTOP,
  TASKLETSTART,
  TASKLETSTOP,
  ENQUEUE,
  DEQUEUE,

  WAITCOMPL,
  COMPL,
  SEMUP,
  SEMDOWN,
  TEMPSYNCH,

  // OYSTEDAL
  MIGRATE,

  STARTIDS, /* Not used, only a separator */

  /* Used internally by sepextract to trace static IDs */
  ID_QUEUE,
  ID_SYNCH,
  ID_STATE,

  __TYPE_LAST = 0
};

#ifdef USNL

char *typestrings[] = {"CTXSW     ",
		       "SRVENTRY  ",
		       "SRVEXIT   ",
		       "PEUSTART  ",
		       "PEUSTOP   ",
		       "PKTEXTR   ",
		       "PKTEXTRN  ",
		       "PKTSTART  ",
		       "PKTSTOP   ",
		       "PKTENQ    ",
		       "PKTDEQ    ",
		       "LOOPSTART ",
		       "LOOPRSTART",
		       "LOOPSTOP  ",
		       "QUEUECOND ",
		       "THREADCOND",
		       "STATECOND ",
		       "CONDEND   ",
		       "PKTQUEUE  ",
		       "PKTQUEUEN ",
		       "SRVQUEUE  ",
		       "STATEQUEUE",

		       "TTWAKEUP  ",
		       "WAKEUP    ",
		       "SIRQRAISE ",
		       "SLEEP     ",
		       "HIRQENTRY ",
		       "HIRQEXIT  ",
		       "SIRQENTRY ",
		       "SIRQEXIT  ",
		       "TSKLENTRY ",
		       "TSKLEXIT  ",
		       "ENQUEUE   ",
		       "DEQUEUE   ",

                       "WAITCOMPL ",
                       "COMPL     ",
                       "SEMUP     ",
                       "SEMDOWN   ",
		       "TEMPSYNCH ",

               "MIGRATE",

		       "----------",

		       /* Used internally by sepextract to trace static IDs */
		       "ID_QUEUE",
		       "ID_SYNCH",
		       "ID_STATE",
};

#endif

/* Structure to store entries */
struct sepentry {
  enum sepentry_type type;
  unsigned int cycles;
  unsigned long pid1;
  unsigned long pid2;
  unsigned int data;
  void *addr;
  void *location;
  unsigned short currpid;
  unsigned short cpu;
};

/* Commands to control the traces */
#define SEPEXT_STOP 1

#ifndef USNL

/* To store trace */
extern unsigned int avail_entries;
extern struct sepentry *entries;
extern unsigned int lastread;
extern unsigned int tracing_active;

/* For statistics */
extern struct sepext_stats stats;

/* Tracing */
int initialize_tracer(unsigned int num_entries, struct module *module);
void cleanup_tracer(void);
void trace_entry(void *location,
		 enum sepentry_type type,
		 unsigned short pid1,
		 unsigned short pid2,
		 unsigned int data,
		 void *addr);
void stop_tracing(void);

/* Static probes */
void activate_static_probes(void);
void deactivate_static_probes(void);
void pktextr_trace(unsigned int pktid, void *address);
void pktstart_trace(void);

/* Dynamic probes */
void initialize_dynamic_probing(void);
void cleanup_dynamic_probing(void);

/* To lookup symbols and addresses */
void initialize_st(void);
void cleanup_st(void);

/* For statistics */
struct sepext_stats {
  u32 entrycnt;
  u32 ctxcnt;
  u32 touscnt;
  u32 debug;
};

/* To store packet extractor information.

   queue_id:   The identification of the location at which pktextr is called.
               Set to 0 if extractor not used.
   extract_start: The location in the data of the first byte to extract
   num_bytes: The number of bytes to extract (currently 0-4) */

struct pktextr_info {
  uint32_t service_id;
  uint32_t extract_start;
  uint32_t num_bytes;
};

/* The maximum number of simultaneously active extractors */
#define MAX_EXTRACTORS 100

#endif

#endif
