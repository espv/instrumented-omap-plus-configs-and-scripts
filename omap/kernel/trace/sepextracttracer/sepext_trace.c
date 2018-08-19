#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <net/netlink.h>
#include <net/net_namespace.h>
#include <linux/percpu.h>
#include <asm/local.h>
#include <trace/sepextract-structures.h>

#include "sepext_trace.h"

struct sock *nl_sk = NULL;
u32 us_pid = 0;
struct proc_dir_entry *proc_sepextcmd = NULL;

static atomic_t lastentry = ATOMIC_INIT(0);
static volatile long unsigned int tasklet_scheduled;
static volatile long unsigned int stop_tracing_requested;
static int buffer_filling;
// int debug = 0;
struct timer_list warmuptimer;

/* For stats */
struct proc_dir_entry *getstats;

void send_to_userspace(void *data, unsigned int length, u32 pid) {
  struct sk_buff *skb_out;
  struct nlmsghdr *nlh;  
  int res;

  /* Send one entry back to user space */  
  skb_out = nlmsg_new(length, 0);
  if(!skb_out)
    {
      printk(KERN_ERR "Failed to allocate new skb\n");
      return;
    }

  nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,length,0);
  memcpy(nlmsg_data(nlh),data,length);
  NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */

  res=nlmsg_unicast(nl_sk, skb_out, pid);

  if(res<0)
    printk(KERN_INFO "Error while sending back to user %d\n", res);
}

/*void activate_tracing_timer(unsigned long data) {
      printk("Were back in buissiness!\n");
      tracing_active = 1;
}

void start_tracing_in(unsigned long when) {
      warmuptimer.expires = jiffies + when;
      warmuptimer.data = 0;  
      warmuptimer.function = activate_tracing_timer;  
      printk("Warming up for %d jiffies\n", WARMUPTIME);
      add_timer(&warmuptimer);
      }*/

void trace_IDs(void) {
  int i;

  /* Trace packet queue IDs */
  for(i = PKTQ_NOLOC; i < QUEUE_LASTREGISTERED; i++)
    trace_entry(0, ID_QUEUE, i, 0, 0, sem_queues[i].head);

  /* Trace synchronization variable IDs */
  for(i = SYNCH_TEMPORARY; i < SYNCH_LASTREGISTERED; i++)
    trace_entry(0, ID_SYNCH, i, 0, 0, sem_synchvars[i].variable);

  /* Trace state variable IDs */
  for(i = STATE_IDENTIFYVIADEVICEFILE; i < STATE_LASTREGISTERED; i++)
    trace_entry(0, ID_STATE, i, 0, 0, 0);
}

void nl_rcv_func (struct sk_buff *skb)
{
  struct nlmsghdr *nlh;  
  /* Obtain PID of requesting process */

  nlh=(struct nlmsghdr*)skb->data;
  us_pid = nlh->nlmsg_pid; /*pid of sending process */

  /* If usnl activates tracing */
  if(*((unsigned int *)NLMSG_DATA(nlh)) == START_TRACING) {
    printk(KERN_INFO "Start tracing\n");
    atomic_set(&lastentry, 0);
    smp_mb();
    tracing_active = 1;

    trace_IDs();

    // start_tracing_in(WARMUPTIME);
    return;
  }
  /* If usnl deactivates tracing */
  else if (*((unsigned int *)NLMSG_DATA(nlh)) == STOP_TRACING) {
    printk(KERN_INFO "Stop tracing\n");
    tasklet_scheduled = 0;
    smp_mb();
    tracing_active = 0;

    lastread = 0;
    atomic_set(&lastentry, 0);

    smp_mb();
    return;
  }
  /* If sync to disk was completed, we can resume tracing. */
  else if(*((unsigned int *)NLMSG_DATA(nlh)) == SYNC_COMPLETE) {
    /* Although it should never happen, we want to make sure
       we don't reset tracing if it is active. Thus, only
       reset lastentry if it is equal to avail_entries. */

      printk(KERN_INFO "Sync complete\n");
      WARN_ON(tracing_active == 1);
    if(!tracing_active) {
      lastread = 0;
      atomic_set(&lastentry, 0);
      tasklet_scheduled = 0;
      smp_mb();
      tracing_active = 1;
      //printk("Warming up before tracing...\n");
      //start_tracing_in(WARMUPTIME);
      return;
    }
  }
  /* If we are out of data, or if we have not yet filled our
     buffer, indicate that. As a result,
     user-space should stop asking for more. */
  else if(*((unsigned int *)NLMSG_DATA(nlh)) == REQUEST_ENTRY) {
    unsigned int message;
    if(stop_tracing_requested && (atomic_read(&lastentry) <= lastread || lastread >= avail_entries)) {
      message = STOP_TRACING;
      send_to_userspace((void *) &message,
			sizeof(unsigned int),
			us_pid);
      stop_tracing_requested = 0;
    }
    else if(tracing_active || lastread >= avail_entries) {
      message = END_OF_DATA;

      send_to_userspace((void *) &message,
			sizeof(unsigned int),
			us_pid);      
    } else {
      stats.touscnt++;
      send_to_userspace((void *) &(entries[lastread]),
			sizeof(struct sepentry),
			us_pid);
      lastread++;
    }
  }
}

/* Tasklet that "awakens" (by sending a message to
   the netlink socket) user space netlink
   app when buffer is full */
void awake_usnl(unsigned long data) {
    unsigned int message = WAKE_UP_USNL;
    printk("Awakening user process\n");
    send_to_userspace((void *) &message,
		      sizeof(struct sepentry),
		      us_pid);    
}

DECLARE_TASKLET(sep_tasklet, &awake_usnl, 0);

static inline u32 armv7pmu_read_counter(int idx)
{
	unsigned long value = 0;

    asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (value));

	return value;
}

void schedule_usnl(void* ignore) {
    smp_mb();

    printk("schedule_usnl() on CPU%d\n", smp_processor_id());

    WARN_ON(tracing_active == 1);
    WARN_ON(test_bit(TASKLET_STATE_RUN, &sep_tasklet.state) == 1);
    WARN_ON(test_bit(TASKLET_STATE_SCHED, &sep_tasklet.state) == 1);

    if (!us_pid) {
        printk("No PID for USNL\n");
        return;
    }

    awake_usnl(0);

    // tasklet_schedule(&sep_tasklet);
    // raise_softirq(TASKLET_SOFTIRQ);

    printk("Scheduling tasklet, state (%lu), disabled (%d)\n",
            sep_tasklet.state,
            atomic_read(&sep_tasklet.count));

}

void trace_entry(void *location,
		 enum sepentry_type type,
		 unsigned short pid1,
		 unsigned short pid2,
		 unsigned int data,
		 void *addr) {
  unsigned long flags, cycles;
  int a;

  //  preempt_disable();
  raw_local_irq_save(flags);

  smp_mb();

  if(tracing_active == 0 && type < STARTIDS) {
    //    preempt_enable();
    raw_local_irq_restore(flags);
    return;
  }
  
  /* We need to use atomic operations here to avoid
     IRQs interrupting threads in this section from
     writing to the same entry.

     TODO: the add no longer needs to be atomic,
     since we have anyway disabled interrupts. */
  //  raw_local_irq_save(flags);
  a = atomic_add_return(1, &lastentry);

  if(a > avail_entries) {
      tracing_active = 0;
      smp_mb();

      printk(KERN_INFO "Trace buffer full\n");
      if (!test_and_set_bit(0, &tasklet_scheduled)) {
          raw_local_irq_restore(flags);
          printk(KERN_INFO "Scheduling \"tasklet\"\n");
          smp_call_function_single(0, &schedule_usnl, NULL, 0);
      } else {
          printk(KERN_INFO "\"Tasklet\" already scheduled\n");
          raw_local_irq_restore(flags);
      }

      // preempt_enable();
      return;
  }


  /* Read cycle counter (TODO: all four event counters) */
#if 1
  cycles = 0;
  asm volatile ("MRC p15, 0, %0, c9, c13, 0\t\n": "=r"(cycles));
#else
  cycles = armv7pmu_read_counter(0);
#endif

  raw_local_irq_restore(flags);
  //  preempt_enable();
  entries[a - 1].cycles = cycles;

  entries[a - 1].type = type;
  entries[a - 1].currpid = current->pid;
  entries[a - 1].pid1 = pid1;
  entries[a - 1].pid2 = pid2;
  entries[a - 1].data = data;
  entries[a - 1].addr = addr;
  entries[a - 1].location = location;
  entries[a - 1].cpu = smp_processor_id();

  stats.entrycnt++;

  /* Reset event counters and cycle counter to zero */
  /* asm volatile("  MRC     p15, 0, r0, c9, c12, 0"); */
  /* asm volatile("  ORR     r0, r0, #0x07"); */
  /* asm volatile("  MCR     p15, 0, r0, c9, c12, 0"); */

}

void stop_tracing(void) {
  unsigned long flags;

  preempt_disable();
  raw_local_irq_save(flags);
  
  /* Stop tracing, request that tracing should be stopped,
     and schedule tasklet to wake usnl
     if not allready schduled. */
  tracing_active = 0;
  stop_tracing_requested = 1;

  if(!test_and_set_bit(0, &tasklet_scheduled))
    if(us_pid) tasklet_schedule(&sep_tasklet);

  raw_local_irq_restore(flags);
  preempt_enable();
}


static int printstats(struct inode *inode, struct file *file)
{
  printk("stats.entrycnt = %d\n", stats.entrycnt);
  printk("stats.ctxcnt = %d\n", stats.ctxcnt);
  printk("stats.touscnt = %d\n", stats.touscnt);
  printk("stats.debug = %d\n", stats.debug);
  return -EPERM;
}

static const struct file_operations statsops = {
	.open = printstats,
	.llseek = no_llseek,
};

int run_command(struct file *file, const char __user *buffer, unsigned long count, void *data ) {
  int cmd = 0;
  int ret;
  char cmdstr[5];

  if(count > 5) {
    printk("SEPEXTRACT: Command should be specified as a single digit\n");
    return -1;
  }

  ret = copy_from_user(cmdstr, buffer, count);
  if(ret) {
    printk("SEPEXTRACT: Could not copy command from user space interpret command\n");
    return -1;
  }

  sscanf(buffer, "%d", &cmd);
  if(cmd == 0) {
    printk("SEPEXTRACT: You did not provide a digit as a command\n");
    return -1;
  }

  printk("SEPEXTRACT: Command %d received\n", cmd);

  if(cmd == SEPEXT_STOP) {
    printk("SEPEXTRACT: Stopping tracer\n");
    stop_tracing();
  }

  return count;
}

int read_command(char *buf, char **start, off_t offset, int count, int *eof ) {
  *eof = 1;
  return 0;  
}

int initialize_tracer(unsigned int num_entries, struct module *module) {
  printk("SEPEXT initialize_tracer()\n");

  /* Initialize timer */
  init_timer(&warmuptimer);

  /* Initialize procfile used to obtain statistics */
  getstats = proc_create("sepextstats", 0444, NULL,
			      &statsops);
  if (!getstats)
    return -EPERM;

  /* Create proc file for executing commands */
  proc_sepextcmd = create_proc_entry("sepextcmd",
				 S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH,
				 NULL); 
  if (proc_sepextcmd)
    {
      /* Setup the Read and Write functions */
      proc_sepextcmd->read_proc  = (read_proc_t *) &read_command;
      proc_sepextcmd->write_proc = (write_proc_t *) &run_command;
    }


  /* Allocate memory for internal buffer */
  entries = (struct sepentry *) vmalloc(sizeof(struct sepentry) * num_entries);
  if(!entries) {
    printk("Could not allocate enough memory for %d entries\n",
	   num_entries);
    return 1;
  }

  printk("      Allocated %d bytes for buffer \n",
	 sizeof(struct sepentry) * num_entries);

  lastread = buffer_filling = 0;
  avail_entries = num_entries;
  tasklet_scheduled = 0;
  smp_mb();
  tracing_active = 0;
  stop_tracing_requested = 0;

  /* Initialize netlink */
  nl_sk = netlink_kernel_create(&init_net,
                                NETLINK_NICTEST,
                                0,
                                nl_rcv_func,
                                NULL,
                                module);

  if(!nl_sk)  
    {   
      printk("Error creating netlink socket.\n");  
      return 1;
      }

  return 0;
}

void cleanup_tracer() {
  printk("Removing proc entries\n");
  remove_proc_entry("sepextstats", NULL);
  remove_proc_entry("sepextcmd", NULL);
  printk("Freeing memory\n");
  vfree(entries);
  printk("Releasing netlink\n");
  netlink_kernel_release(nl_sk);
  printk("Removing tasklet\n");
  tasklet_kill(&sep_tasklet);
}
