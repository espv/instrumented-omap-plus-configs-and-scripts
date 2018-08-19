/* Trace kmalloc.  Taken basically from Documentation/kprobes.txt */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/ftrace.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <net/sock.h>
#include <linux/kprobes.h>
#include <trace/sepextract.h>
#include <asm/byteorder.h>
#include "sepext_trace.h"
#include <linux/tracepoint.h>

#include <asm/pmu.h>

// #include <arch/arm/mach-omap2/include/

#include <asm/io.h>

// unsigned int numentries = 20000;
unsigned int numentries = 100000;
// unsigned int numentries = 1000; // OYSTEDAL: Concurrency stress test
module_param(numentries, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

/* NEXT: - Unregister static traceprobes
         - Register also the dynamic probes

   NOTE: Do not explicitly provide functions to obtain
         packet sequence numbers - the hypothesis is that
         this can be extracted from the trace afterwards
	 if we assume queues are FIFO (which is the
	 most common case). If it is not FIFO, information
	 about the de-queueing policy can be provided. In
	 cases this policy is not deterministic, the user
	 must provide a function to extract which packet
	 this corresponds to. The latter case provides
	 a challenge since we do not explicitly store
	 the packet id during runtime, so this must be
	 somehow extracted during the offline workflow
	 analysis mentioned above.
*/

static int __init sepext_init_module(void)
{
  printk("Loading and initializing SEPExtract:\n");

  printk("   Activating tracer\n");
  if(initialize_tracer(numentries, THIS_MODULE)) {
    printk("Could not initialize tracer\n");
    return -10;
  }

  printk("   Activating static probes\n");
  activate_static_probes();
  printk("   Activating dynamic probing\n");
  initialize_dynamic_probing();
  printk("   Initializing symbol lookup\n");
  initialize_st();

  printk("SEPExtract loading done\n");

  return 0;
}

void cleanup_module(void)
{
  printk(KERN_INFO "Cleaning up and unloading SEPExtract:\n");

  printk("   Deactivating dynamic probing\n");
  cleanup_dynamic_probing();
  printk("   Deactivating static probes\n");
  deactivate_static_probes();
  printk("   Deactivating tracer\n");
  cleanup_tracer();
  tracepoint_synchronize_unregister();
  printk("   Removing symbol lookup\n");
  cleanup_st();
}

module_init(sepext_init_module);
module_exit(cleanup_module);

MODULE_LICENSE("GPL");
