#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include "sepext_trace.h"

#define MAX_PROBES 100
struct kprobeEntry {
  struct kretprobe rp;
  struct jprobe jp;
};

struct kprobeEntry kprobes[MAX_PROBES];
unsigned int lastprobe;

#define MEMADDR_STRINGSIZE 10
struct proc_dir_entry *proc_trace = NULL;
char procfsbuffer[(MEMADDR_STRINGSIZE * 2) + 1 + 1];

int entryhandler(struct kretprobe_instance *ri, struct pt_regs *regs) {
  trace_entry(ri->ret_addr, SERVICEENTRY, 0, 0, 0, ri->rp->kp.addr);
  return 0;
}

int returnhandler(struct kretprobe_instance *ri, struct pt_regs *regs) {
  trace_entry(ri->ret_addr, SERVICEEXIT, 0, 0, 0, ri->rp->kp.addr);
  return 0;
}

int add_probe(struct file *file, const char __user *buffer, unsigned long count, void *data ) {
  void *address = 0;
  void *pktidaddress = 0;
  unsigned int ret;

  /* TODO: Add error message */
  if(count > (MEMADDR_STRINGSIZE * 2) + 1 + 1) {
    printk("Sepextract: Too long symbol name (%lu bytes)\n", count);
    return -1;
  }

  procfsbuffer[count] = '\0';
  ret = copy_from_user(procfsbuffer, buffer, count);
  if(ret) {
    printk("Sepextract: could not copy dynprobe address from user space\n");
    return -1;
  }

  sscanf(procfsbuffer, "%x %x", &address, &pktidaddress);

  if(address == 0) {
    printk("Sepextract: Malformed address: %s\n", procfsbuffer);
    return -1;
  }

  printk("ADD PROBE TO %px WITH PACKET EXTRACTOR %px\n",
	 address, pktidaddress);

  /* Register kprobe */
  if(lastprobe < (MAX_PROBES - 1)) {
    /* Register kretprobe (+ entry?) first */
    kprobes[lastprobe].rp.handler = returnhandler;
    kprobes[lastprobe].rp.entry_handler = entryhandler;
    kprobes[lastprobe].rp.kp.addr = address;
    
    if ((ret = register_kretprobe(&(kprobes[lastprobe].rp))) < 0) {
      printk("Register_kretprobe at %px failed, returned %d\n", address, ret);
      unregister_kretprobe(&(kprobes[lastprobe].rp));
      return -1;
    }

    /* If we were provided with an extra address, install
       a jprobe to that address. This second address should
        be that of a function returning packet ID. */
    
    if(pktidaddress != 0) {
      kprobes[lastprobe].jp.entry = pktidaddress;
      kprobes[lastprobe].jp.kp.addr = address;
      
      if ((ret = register_jprobe(&(kprobes[lastprobe].jp))) < 0) {
	printk("Register_jprobe at %px failed, returned %d\n", address, ret);
	unregister_jprobe(&(kprobes[lastprobe].jp));
	return -1;
      }
    }

    /* We successfully installed at least one probe,
       so we must increase the index to the array */
      lastprobe++;
  } else {
    printk("Sepextract: too many dynamic probes\n");
    return -1;
  }

  return count;
}

int read_probes(char *buf, char **start, off_t offset, int count, int *eof ) {
  *eof = 1;
  return 0;  
}

void initialize_dynamic_probing() {
  /* Clear probes */
  memset(kprobes, 0, sizeof(struct kprobeEntry) * MAX_PROBES);
  lastprobe = 0;

  /* Create a proc file for outgoing packets */
  proc_trace = create_proc_entry("dynprobes",
				 S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH,
				 NULL); 
  if (proc_trace)
    {
      /* Setup the Read and Write functions */
      proc_trace->read_proc  = (read_proc_t *) &read_probes;
      proc_trace->write_proc = (write_proc_t *) &add_probe;
    }
}

void cleanup_dynamic_probing() {
  int i;
  /* Remove proc entry */
  remove_proc_entry( "dynprobes", NULL );

  /* Unregister probes */
  i = 0;
  for(; i <= lastprobe; i++) {
    if (kprobes[i].rp.kp.addr)
      unregister_kretprobe(&(kprobes[i].rp));

    if(kprobes[i].jp.kp.addr)
      unregister_jprobe(&(kprobes[i].jp));
  }
}
