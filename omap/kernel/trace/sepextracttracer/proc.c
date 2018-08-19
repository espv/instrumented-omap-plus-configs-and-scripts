#define MAX_PROBES 100
struct kprobeEntry {
  struct kprobe kp;
  int (*seqHandler) (unsigned long addr);
}
struct kprobe kprobes[MAX_PROBES];
unsigned int lastprobe;

/* Procfs entry and the buffer used to store probe descriptions.
   NS_FUNCS = the number of functions to provide (now: service, pktidfunc)
   PROBEDESCSIZE = the number of bytes used for the textual probe description
                   provided from user-space */
#define NR_FUNCS 2
#define PROBEDESCSIZE (NR_FUNCS * (sizeof(kprobe_opcode_t) * 2) + NR_FUNCS)
struct proc_dir_entry *proc_trace = NULL;
char procfsbuffer[PROBEDESCSIZE + 1];

void init_dynamic(void) {
  /* Create a proc file for outgoing packets */
  proc_trace = create_proc_entry("dynprobes", S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH, NULL); 
  if (proc_trace)
    {
      /* Setup the Read and Write functions */
      proc_trace->read_proc  = read_probes;
      proc_trace->write_proc = add_probe;
    }
}

void cleanup_dynamic(void) {
  /* Remove proc entry */
  remove_proc_entry( "dynprobes", NULL );

  /* Unregister probes */
  int i = 0;
  for(; i <= lastprobe; i++) {
    unregister_kprobe(&(kprobes[i]));
  }
}
