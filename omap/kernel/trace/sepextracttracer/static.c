#include <trace/sepextract.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include "sepext_trace.h"
#include "linux/sched.h"
#include "linux/skbuff.h"

#include "linux/if_ether.h"
#include "linux/ip.h"
#include "linux/udp.h"

/* For debug */ 
#include "linux/stacktrace.h"

struct pktextr_info pi_entries[MAX_EXTRACTORS];
int num_pi_entries;

int doStackTrace; /* For debugging */

/* Size of string to request packet extractor:
   service_id: 32-bit value (up to billions = 10 digits)
   Whitespace
   location of extract in packet: 32-bit value
   Whitespace
   Nr. of bytes to extract (1-4): 1 digit
   Newline: potentially 2 bytes
*/
#define PKTEXTR_STRINGSIZE 10 + 1 + 10 + 1 + 1 + 2
struct proc_dir_entry *proc_pktextr = NULL;
char pktextrprocbuffer[PKTEXTR_STRINGSIZE];

/*******************************/
/* PROTOCOL SERVICES:          */
/*******************************/
void probe_sepext_pktqueue(void* ignore, void *pktdata, unsigned int queue, void *head, enum sem_queue_operation op) 
{
    enum sepentry_type typeToTrace;
    int extracted = 0;
    int i = 0;

    /* If pktdata == NULL, we know we are at a point where
       the packet is not yet available (e.g., due to not yet
       being fetched from a PEU, e.g., the NIC), or we have
       de de-queue which did not yield a poacket. In this case,
       we simply store 0 */
    if(pktdata == NULL) {
        trace_entry(__builtin_return_address(0), PKTQUEUE, op, queue, 0, (void *) head);
        return;
    }

    /* Search for the packet extractor */
    if(queue != 0)
        for(; i < num_pi_entries; i++) {
            if(pi_entries[i].service_id == queue)
            {
                typeToTrace = (extracted == 1 ? PKTQUEUEN : PKTQUEUE);
                /* num_bytes cannot be 3, as this is not one of the known
                   data types (i.e., no data type has 3 bytes). Thus, it
                   does not typically occurr in packet headers. */
                if(pi_entries[i].num_bytes == 1)
                    trace_entry(__builtin_return_address(0), typeToTrace, (unsigned short) op, (unsigned short) queue, *((uint8_t *)(pktdata + pi_entries[i].extract_start)), (void *) head);
                else if(pi_entries[i].num_bytes == 2)
                    trace_entry(__builtin_return_address(0), typeToTrace, (unsigned short) op, (unsigned short) queue, *((uint16_t *)(pktdata + pi_entries[i].extract_start)), (void *) head);
                else
                    trace_entry(__builtin_return_address(0), typeToTrace, (unsigned short) op, (unsigned short) queue, *((uint32_t *)(pktdata + pi_entries[i].extract_start)), (void *) head);

                extracted = 1;
            }
        }
    else if (head != NULL)
        for(; i < num_pi_entries; i++) {
            if(sem_queues[pi_entries[i].service_id].head == head)
            {
                typeToTrace = (extracted == 1 ? PKTQUEUEN : PKTQUEUE);
                /* num_bytes cannot be 3, as this is not one of the known
                   data types (i.e., no data type has 3 bytes). Thus, it
                   does not typically occurr in packet headers. */
                if(pi_entries[i].num_bytes == 1)
                    trace_entry(__builtin_return_address(0), typeToTrace, (unsigned short) op, (unsigned short) queue, *((uint8_t *)(pktdata + pi_entries[i].extract_start)), (void *) head);
                else if(pi_entries[i].num_bytes == 2)
                    trace_entry(__builtin_return_address(0), typeToTrace, (unsigned short) op, (unsigned short) queue, *((uint16_t *)(pktdata + pi_entries[i].extract_start)), (void *) head);
                else
                    trace_entry(__builtin_return_address(0), typeToTrace, (unsigned short) op, (unsigned short) queue, *((uint32_t *)(pktdata + pi_entries[i].extract_start)), (void *) head);

                extracted = 1;
            }
        }


    /* If we has at least one extracotor, return. */
    if(extracted)
        return;

    /* Here we have a packet, but no specified packet extractor. In this case, we trace the packet pointer. */
    trace_entry(__builtin_return_address(0), PKTQUEUE, (unsigned short) op, queue, 0, (void *) head);
}
void probe_sepext_srventry(void *ignore, void *address)
{
  trace_entry(__builtin_return_address(0), SERVICEENTRY, 0, 0, 0, address);
}
void probe_sepext_srvexit(void *ignore, void *address)
{
  trace_entry(__builtin_return_address(0), SERVICEEXIT, 0, 0, 0, address);
}

void probe_sepext_peustart(void *ignore, void *address)
{
  trace_entry(__builtin_return_address(0), PEUSTART, 0, 0, 0, address);
}

void probe_sepext_peustop(void *ignore, void *address)
{
  trace_entry(__builtin_return_address(0), PEUSTOP, 0, 0, 0, address);
}

void probe_sepext_looprestart(void *ignore, void *dummy) {
  trace_entry(__builtin_return_address(0), LOOPRESTART, 0, 0, 0, 0);
}
void probe_sepext_loopstart(void *data, int maxIterations, short perQueue, unsigned int from, unsigned int to) {
  trace_entry(__builtin_return_address(0), LOOPSTART, perQueue, from, to, (void *)maxIterations);
}

void probe_sepext_loopstop(void *ignore, void *dummy) {
  trace_entry(__builtin_return_address(0), LOOPSTOP, 0, 0, 0, 0);
}

void probe_sepext_queuecondition(void *data, enum sem_queue_condition condition, unsigned int from, unsigned int to) {
  trace_entry(__builtin_return_address(0), QUEUECOND, from, to, (unsigned int) condition, 0);
}
void probe_sepext_threadcondition(void *data, enum sem_thread_condition condition, unsigned long pid) {
  trace_entry(__builtin_return_address(0), THREADCOND, pid, 0, (unsigned int) condition, 0);
}
void probe_sepext_statecondition(void *ignore, enum sem_state_id condition, unsigned long value, unsigned int readwrite, unsigned int global) {
  trace_entry(__builtin_return_address(0), STATECOND, readwrite, global, (unsigned int) value, (void *) condition);
}

// from pktgen.c
struct pktgen_hdr {
	__be32 pgh_magic;
	__be32 seq_num;
	__be32 tv_sec;
	__be32 tv_usec;
};

void probe_sepext_pktextract(void *ignore, void* value, unsigned int length, unsigned int global)
{
#if 1
    if (global == 1 || global == 2) {
        // This happens when the packet is received and ready to be sent to the IP layer.
        struct sk_buff* skb = (struct sk_buff*)value;

        // printk("pktextract\n");

        // printk("protocol ho: %d\n", ntohs(skb->protocol));
        // printk("protocol no: %d\n", skb->protocol);

        if (ntohs(skb->protocol) != ETH_P_IP) {
            return; // Not ip packet
        }

        // printk("Found IP header\n");

        struct iphdr* ip_header = ip_hdr(skb);

        if (ip_header->version != 4) {
            // printk("... but not version 4\n");
            return;
        }

        printk("protocol ho: %d\n", ntohs(ip_header->protocol));
        printk("protocol no: %d\n", ip_header->protocol);

        if (ip_header->protocol != IPPROTO_UDP) {
            return; // Not udp packet
        }

        unsigned int ip_hdr_len = ip_header->ihl * 4; // given in no. of 32-bit words

        printk("Header length: %d\n", ip_hdr_len);

        struct udphdr* udp_hdr = ((char*)ip_header) + ip_hdr_len;

        printk("UDP src: %d  dst: %d  len: %d\n",
                ntohs(udp_hdr->source),
                ntohs(udp_hdr->dest),
                ntohs(udp_hdr->len));

        BUG_ON(sizeof(struct udphdr) != 8);
        struct pktgen_hdr* pg_hdr = ((char*)udp_hdr) + sizeof(struct udphdr);

        printk("PKTGEN magic: 0x%x %s  seqno: %d\n",
                ntohl(pg_hdr->pgh_magic),
                ntohl(pg_hdr->pgh_magic) == 0xbe9be955 ? "(correct)" : "(incorrect)",
                ntohl(pg_hdr->seq_num));


        if (ntohl(pg_hdr->pgh_magic) != 0xbe9be955) {
            return; // Not a pktgen packet
        }

        // TODO: Write udp port no. for stream identification
        trace_entry(__builtin_return_address(0), PKTEXTRACT, udp_hdr->source, (unsigned long)global, ntohl(pg_hdr->seq_num) + 1, 0);
    } else {
        // Always trace other events
        trace_entry(__builtin_return_address(0), PKTEXTRACT, 0, (unsigned long)global, 0, 0);
    }

    /*
    // Always trace RX
    if (length == 0) {
        trace_entry(__builtin_return_address(0), PKTEXTRACT, 0, (unsigned long)global, (unsigned int) value, 0);
    } else {
        if (length < 200 && length >= 20 + 8 + 8) { // IP + UDP header + PKTGEN(magic and seqno)
        // if (length == 100) { // IP + UDP header + PKTGEN(magic and seqno)
            uint32_t* ptr = (uint32_t*)value;

            if (*(ptr+7) == htonl(0xbe9be955)) {
                printk("PKTGEN Packet seqno. %d case: %lu\n", ntohl(*(ptr+8)), global);
            } else {
                printk("Unknown packet of length %d, case %lu\n", length, global);
                
                printk(">>>");
                int i;
                for(i = 0; i < length/4; i++) {
                    printk("%x ", ptr[i]);
                }
                printk("<<<\n");
            }
        }

        trace_entry(__builtin_return_address(0), PKTEXTRACT, 0, (unsigned long)global, (unsigned int) value, 0);
    }
    */
#else 
    trace_entry(__builtin_return_address(0), PKTEXTRACT, 0, (unsigned long)global, (unsigned int) value, 0);
#endif
}
void probe_sepext_conditionend(void) {
  trace_entry(__builtin_return_address(0), CONDEND, 0, 0, 0, 0);
}
void probe_sepext_srvqueue(void *ignore, void *address, unsigned int queue, void *head, enum sem_queue_operation operation) {
  trace_entry(__builtin_return_address(0), SRVQUEUE, (unsigned short) operation, (unsigned short) queue,  (unsigned int) head, address);
}

void probe_sepext_statequeue(void* ignored, unsigned int queue, unsigned int value, enum sem_queue_operation operation, unsigned int scope) {
  trace_entry(__builtin_return_address(0), STATEQUEUE, (unsigned short) operation, (unsigned short) queue,  value, (void *) scope);
}


/*******************************/
/* SCHEDULER:                  */
/*******************************/
/* kernel/sched.c: */
void probe_sepext_wakeup(void* ignored, void *location, struct task_struct *p){
  trace_entry(location, TTWAKEUP, p->pid, 0, 0, 0);
}
void probe_sepext_ctxswitch(void* ignore,
			    struct task_struct *prev,
			    struct task_struct *next){
  stats.ctxcnt++;
  trace_entry(0, CTXSWITCH, prev->pid, next->pid, 0, 0);
}

void probe_sepext_migrate(void* ignore, int newCPU, int pid) {
    trace_entry(0, MIGRATE, pid, newCPU, 0, 0);
}

// extern void dump_stack(void); /* For debunning - see below */

void probe_sepext_dequeuerq(void* ignore, void *location, struct task_struct *p, int flags){
  /* Current goal: find peustart on N900 in wl1251_rx() */
#if 0
  if(doStackTrace)
    if(doStackTrace == current->pid)
      dump_stack();
#endif

  trace_entry(location, SLEEP, p->pid, 0, (unsigned int) flags, 0);
}

/*******************************/
/* HARDWARE INTERRUPTS:        */
/*******************************/
/* kernel/irq/handle.c: */
void probe_sepext_hirqstart(void *ignore, unsigned int hirq){
  trace_entry(0, HIRQSTART, hirq, 0, 0, 0);
}
void probe_sepext_hirqstop(void *ignore, unsigned int hirq) {
  trace_entry(0, HIRQSTOP, hirq, 0, 0, 0);
}

/*******************************/
/* SYNCHRONIZATION PRIMITIVES: */
/*******************************/
void probe_sepext_tempsynch(void *ignore, void *location, enum sem_synch_type type, void *variable, enum sem_synch_scope scope) {
  trace_entry(location, TEMPSYNCH, (unsigned int) scope, 0, (unsigned int) type, variable);
}
void probe_sepext_semaphore_up(void *ignore, void *location, struct semaphore *x, enum sem_synch_scope scope) {
  trace_entry(location, SEMUP, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_semaphore_down(void *ignore, void *location, struct semaphore *x, enum sem_synch_scope scope) {
  trace_entry(location, SEMDOWN, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_wait_for_completion(void *ignore, void *location, struct completion *x, enum sem_synch_scope scope) {
  trace_entry(location, WAITCOMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_wait_for_completion_timeout(void *ignore, void *location, struct completion *x, unsigned long timeout, enum sem_synch_scope scope) {
  trace_entry(location, WAITCOMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_wait_for_completion_interruptible(void *ignore, void *location, struct completion *x, enum sem_synch_scope scope) {
  trace_entry(location, WAITCOMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_wait_for_completion_interruptible_timeout(void *ignore, void *location, struct completion *x, unsigned long timeout, enum sem_synch_scope scope) {
  trace_entry(location, WAITCOMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_wait_for_completion_killable(void *ignore, void *location, struct completion *x, enum sem_synch_scope scope) {
  trace_entry(location, WAITCOMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_wait_for_completion_killable_timeout(void *ignore, void *location, struct completion *x, unsigned long timeout, enum sem_synch_scope scope) {
  trace_entry(location, WAITCOMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_try_wait_for_completion(void *ignore, void *location, struct completion *x, enum sem_synch_scope scope) {
  trace_entry(location, WAITCOMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_complete(void *ignore, void *location, struct completion *x, enum sem_synch_scope scope) {
  trace_entry(location, COMPL, (unsigned int) scope, 0, 0, x);
}
void probe_sepext_complete_all(void *ignore, void *location, struct completion *x, enum sem_synch_scope scope) {
  trace_entry(location, COMPL, (unsigned int) scope, 0, 0, x);
}

/*******************************/
/* DEBUG:                      */
/*******************************/
void probe_sepext_debug(unsigned long data) {
  if(data)
    doStackTrace = current->pid;
  else
    doStackTrace = 0;
}


/*******************************/
/* DEPRECATED:                 */
/*******************************/
/* Deprecated because we use one function for pkt queues with an
   attribute specifying en- or de-queu */
void probe_sepext_pktenq(void *address, void *data) {
  trace_entry(__builtin_return_address(0), PKTENQ, 0, 0, 0, address);
}
void probe_sepext_pktdeq(void *address, void *data) {
  trace_entry(__builtin_return_address(0), PKTDEQ, 0, 0, 0, address);
}

/* Deprecated because we assume change of packets only between
   de-queues in order to simplify. Usually holds. */ 
void probe_sepext_pktstart(void *address, void *data) {
  trace_entry(__builtin_return_address(0), PKTSTART, 0, 0, 0, 0);
}
void probe_sepext_pktstop(void *address, void *data) {
  trace_entry(__builtin_return_address(0), PKTSTOP, 0, 0, 0, 0);
}

/* Deprecated because what matters is try_to_wake_up() */
void probe_sepext_enqueuerq(
			    void *location,
			    struct task_struct *p,
			    int flags){
  trace_entry(location, WAKEUP, p->pid, 0, (unsigned int) flags, 0);
}

/* TODO: include/linux/list.h

   NOTE: First, run N times to obtain which
   queues are inserted into. We must here
   filter wrt. the service of interrest. A
   challenge here is to not include insertions
   related to the scheduler. How do we do this?
   Anyway, when we have obtained these queues,
   this function should be set up to discriminate
   one of these at a time, each one during N
   service executions. The same applies to the
   skbuff lists.

   NOTE2: No, we should try to avoid this. It is
   too complicated, and after thinking about
   excluding scheduler related queueing, I have
   not yet found a good way to do this. Furthermore,
   there may be additional activity which uses
   queues, and this becomes messy! In stead, we
   should strive to select proper "protocol/component
   dependent" enqueue/dequeue functions to trace.
   That is, in spite of the difficulty involved due
   to dynamic linking of objects, functions and
   queues. The challenge then becomes how to
   find the proper function, which in turn
   translates into unveiling component/protocol
   boundaries/APIs. What determines this is that
   (1) many, and (2) if code is replaced, it is
   likely that it will be this whole component.
   Hence, it all results in the requirement of
   manual modelling of components and interactions
   between them. */
void probe_list_add_tail(
			 struct list_head *new,
			 struct list_head *head) {
  trace_entry(__builtin_return_address(0), ENQUEUE, 0, 0, 0, head);
}

/* TODO: include/linux/skbuff.h

   NOTE: We want this to be static as the sockbuff
   list API is heavily used as a communication
   means between protocols. And it is not
   likely to change with the protocol stack, as
   that would break all other protocols using this
   API. */
/* kernel/softirq.c. DEPRECATED because we use service queues in stead */
void probe_sepext_softirqraise(unsigned int nr){
  trace_entry(__builtin_return_address(0), SIRQRAISE, nr, 0, 0, 0);
} /* PIOTR: build return address from 1 to 0, argument 1 is not supported */

void probe_sepext_softirqstart(unsigned int sirq){
  trace_entry(__builtin_return_address(0), SOFTIRQSTART, sirq, 0, 0, 0);
}
void probe_sepext_softirqstop(unsigned int sirq) {
  trace_entry(__builtin_return_address(0), SOFTIRQSTOP, sirq, 0, 0, 0);
}
void probe_sepext_taskletstart(unsigned long tasklet) {
  trace_entry(__builtin_return_address(0), TASKLETSTART, tasklet, 0, 0, 0);
}
void probe_sepext_taskletstop(unsigned long tasklet) {
  trace_entry(__builtin_return_address(0), TASKLETSTOP, tasklet, 0, 0, 0);
}


/*******************************/
/* END OF PROBES               */
/*******************************/

int add_pktextr(struct file *file, const char __user *buffer, unsigned long count, void *data ) {
  uint32_t service_id = 0;
  uint32_t extract_start = 0;
  uint32_t num_bytes = 0;
  unsigned int ret;

  /* Make sure we do not activate too many extractors */
  if(num_pi_entries >= MAX_EXTRACTORS) {
    printk("Sepextract: no more packet extractors can be activated (max %u)\n",
	   MAX_EXTRACTORS);
    return -1;
  }

  /* TODO: Add error message */
  if(count > PKTEXTR_STRINGSIZE) {
    printk("Sepextract: Too long string to specify packet extractor name (%lu bytes)\n", count);
    return -1;
  }

  pktextrprocbuffer[count] = '\0';
  ret = copy_from_user(pktextrprocbuffer, buffer, count);
  if(ret) {
    printk("Sepextract: could not copy packet extractor specification from user space\n");
    return -1;
  }

  ret = sscanf(pktextrprocbuffer, "%u %u %u", &service_id, &extract_start, &num_bytes);
  
  if(ret < 3) {
    printk("Sepextract: packet extractor specification: %s (should be 3 numbers)\n", pktextrprocbuffer);
    return -1;
  }
  
  printk("Activating packet extractor no. %u with id %u, obtaining %u bytes from packet location %u\n",
	 num_pi_entries, service_id, num_bytes, extract_start);

  /* Actually insert the entry. */
  pi_entries[num_pi_entries].service_id = service_id;
  pi_entries[num_pi_entries].extract_start = extract_start;
  pi_entries[num_pi_entries].num_bytes = num_bytes;

  num_pi_entries++;
  
  return count;
}

int read_pktextr(char *buf, char **start, off_t offset, int count, int *eof ) {
  printk("List of active packet extractors:\n");
  *eof = 1;
  return 0;  
}

#define REGISTER_SEPEXT_TRACE(x) \
    do { \
        int ret = register_trace_sepext_##x(probe_sepext_##x, NULL); \
        if (ret) { \
            printk("Probe activation of trace_sepext_##x failed with %d\n", ret); \
            unregister_trace_sepext_##x(probe_sepext_##x, NULL); \
        } \
    } while(0);

/* ACTIVATION */
void activate_static_probes() {
    num_pi_entries = 0;
    doStackTrace = 0; /* For debugging */ 

    /* Register proc entry for activating packet extractors */
    proc_pktextr = create_proc_entry("pktextr",
            S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH,
            NULL); 
    if (proc_pktextr)
    {
        /* Setup the Read and Write functions */
        proc_pktextr->read_proc  = (read_proc_t *) &read_pktextr;
        proc_pktextr->write_proc = (write_proc_t *) &add_pktextr;
    }

#ifdef CONFIG_SEPEXT_TRACEPOINTS_ENABLED
    REGISTER_SEPEXT_TRACE(srventry);
    REGISTER_SEPEXT_TRACE(srvexit);

    /* kernel/irq/handle.c: */ 
    REGISTER_SEPEXT_TRACE(hirqstart);
    REGISTER_SEPEXT_TRACE(hirqstop);
    REGISTER_SEPEXT_TRACE(ctxswitch);
    REGISTER_SEPEXT_TRACE(peustart);
    REGISTER_SEPEXT_TRACE(peustop);
    REGISTER_SEPEXT_TRACE(pktqueue);
    REGISTER_SEPEXT_TRACE(srvqueue);
    REGISTER_SEPEXT_TRACE(loopstart);
    REGISTER_SEPEXT_TRACE(looprestart);
    REGISTER_SEPEXT_TRACE(loopstop);
    REGISTER_SEPEXT_TRACE(queuecondition);
    REGISTER_SEPEXT_TRACE(statecondition);

    REGISTER_SEPEXT_TRACE(semaphore_up);
    REGISTER_SEPEXT_TRACE(semaphore_down);

    REGISTER_SEPEXT_TRACE(wakeup);
    REGISTER_SEPEXT_TRACE(dequeuerq);

    REGISTER_SEPEXT_TRACE(wait_for_completion);
    REGISTER_SEPEXT_TRACE(wait_for_completion_timeout);
    REGISTER_SEPEXT_TRACE(wait_for_completion_interruptible);
    REGISTER_SEPEXT_TRACE(wait_for_completion_interruptible_timeout);
    REGISTER_SEPEXT_TRACE(wait_for_completion_killable);
    REGISTER_SEPEXT_TRACE(wait_for_completion_killable_timeout);
    REGISTER_SEPEXT_TRACE(try_wait_for_completion);
    REGISTER_SEPEXT_TRACE(complete);
    REGISTER_SEPEXT_TRACE(complete_all);
    REGISTER_SEPEXT_TRACE(tempsynch);
#endif

#ifdef CONFIG_SEPEXT_MIGRATION
    REGISTER_SEPEXT_TRACE(migrate);
#endif

#ifdef CONFIG_SEPEXT_PKTEXTR
    REGISTER_SEPEXT_TRACE(pktextract);
#endif

#if 0
    REGISTER_SEPEXT_TRACE(pktstart);
    REGISTER_SEPEXT_TRACE(pktstop);
    REGISTER_SEPEXT_TRACE(pktenq);
    REGISTER_SEPEXT_TRACE(pktdeq);
    REGISTER_SEPEXT_TRACE(threadcondition);
    REGISTER_SEPEXT_TRACE(statequeue);
    REGISTER_SEPEXT_TRACE(conditionend);

    /* Debug */
    REGISTER_SEPEXT_TRACE(debug);

    /* kernel/sched.c: */

    /* Wakeup must not be used as it is allready
       captured by enqueuerq

       NOT TRUE!: NO - it MUST be captured! As stated in the
paper: "it is important to capture the _attempt_ to
activate, rather than an actual activation. Otherwise,
the event will not be captured if the target thread is
already active".*/
    
    REGISTER_SEPEXT_TRACE(softirqraise);
    REGISTER_SEPEXT_TRACE(enqueuerq);

    ///
    ///

    /* kernel/softirq.c: */ 
    REGISTER_SEPEXT_TRACE(softirqstart);
    REGISTER_SEPEXT_TRACE(softirqstop);
    REGISTER_SEPEXT_TRACE(taskletstart);
    REGISTER_SEPEXT_TRACE(taskletstop);

#endif
}

#define UNREGISTER_SEPEXT_TRACE(x) \
    do { \
       unregister_trace_sepext_##x(probe_sepext_##x, NULL); \
    } while (0);

/* DEACTIVATION */
void deactivate_static_probes() {
    /* Remove proc entry for activating packet extractors */
    remove_proc_entry("pktextr", NULL);  

#ifdef CONFIG_SEPEXT_TRACEPOINTS_ENABLED
    UNREGISTER_SEPEXT_TRACE(wakeup);

    /* PROTOCOL EVENTS: */
    UNREGISTER_SEPEXT_TRACE(srventry);
    UNREGISTER_SEPEXT_TRACE(srvexit);

    /* LEU */
    UNREGISTER_SEPEXT_TRACE(ctxswitch);
    UNREGISTER_SEPEXT_TRACE(hirqstart);
    UNREGISTER_SEPEXT_TRACE(hirqstop);

    UNREGISTER_SEPEXT_TRACE(peustart);
    UNREGISTER_SEPEXT_TRACE(peustop);
    UNREGISTER_SEPEXT_TRACE(pktqueue);

    UNREGISTER_SEPEXT_TRACE(loopstart);
    UNREGISTER_SEPEXT_TRACE(looprestart);
    UNREGISTER_SEPEXT_TRACE(loopstop);
    UNREGISTER_SEPEXT_TRACE(queuecondition);
    UNREGISTER_SEPEXT_TRACE(srvqueue);

    UNREGISTER_SEPEXT_TRACE(semaphore_up);
    UNREGISTER_SEPEXT_TRACE(semaphore_down);
    UNREGISTER_SEPEXT_TRACE(wait_for_completion);
    UNREGISTER_SEPEXT_TRACE(wait_for_completion_timeout);
    UNREGISTER_SEPEXT_TRACE(wait_for_completion_interruptible);
    UNREGISTER_SEPEXT_TRACE(wait_for_completion_interruptible_timeout);
    UNREGISTER_SEPEXT_TRACE(wait_for_completion_killable);
    UNREGISTER_SEPEXT_TRACE(wait_for_completion_killable_timeout);
    UNREGISTER_SEPEXT_TRACE(try_wait_for_completion);
    UNREGISTER_SEPEXT_TRACE(complete);
    UNREGISTER_SEPEXT_TRACE(complete_all);
#endif

#ifdef CONFIG_SEPEXT_MIGRATION
    UNREGISTER_SEPEXT_TRACE(migrate);
#endif

#ifdef CONFIG_SEPEXT_PKTEXTR
    UNREGISTER_SEPEXT_TRACE(pktextract);  
#endif

#if 0 // OYSTEDAL

    UNREGISTER_SEPEXT_TRACE(pktstart);
    UNREGISTER_SEPEXT_TRACE(pktstop); 
    UNREGISTER_SEPEXT_TRACE(pktenq);
    UNREGISTER_SEPEXT_TRACE(pktdeq);
    UNREGISTER_SEPEXT_TRACE(threadcondition);
    UNREGISTER_SEPEXT_TRACE(conditionend);

    UNREGISTER_SEPEXT_TRACE(statequeue);


    UNREGISTER_SEPEXT_TRACE(statecondition);
    UNREGISTER_SEPEXT_TRACE(tempsynch);

    /* Debug */
    UNREGISTER_SEPEXT_TRACE(debug);

    /* LEUs: */
    UNREGISTER_SEPEXT_TRACE(softirqraise);
    UNREGISTER_SEPEXT_TRACE(enqueuerq);
    UNREGISTER_SEPEXT_TRACE(dequeuerq);
    UNREGISTER_SEPEXT_TRACE(softirqstart);
    UNREGISTER_SEPEXT_TRACE(softirqstop);
    UNREGISTER_SEPEXT_TRACE(taskletstart);
    UNREGISTER_SEPEXT_TRACE(taskletstop);

    /* SYNCHRONIZATION: */ 
#endif
}
