#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include "sepext_trace.h"

/* Proc file */
struct proc_dir_entry *st_trace = NULL;

/* Storage for symbol or address provided by user */
#define MEMADDR_STRINGSIZE 255
char stsymbuffer[(MEMADDR_STRINGSIZE * 2) + 1 + 1];
unsigned long address = 0;

/* Determines type of input: address of symbol */
#define ADDRESS 0
#define SYMBOL 1
#define STERROR 2
unsigned int type = 0;

int set_address(struct file *file, const char __user *buffer, unsigned long count, void *data ) {
  unsigned int ret;

  if(count > MEMADDR_STRINGSIZE) {
    printk("Sepextract: symbol must have < 255 characters\n");
    type = STERROR;
    return -1;
  }

  stsymbuffer[count] = '\0';
  ret = copy_from_user(stsymbuffer, buffer, count);
  if(ret) {
    printk("Sepextract: could not copy dynprobe address from user space\n");
    type = STERROR;
    return -1;
  }

  /* Remove prospective newline */
  if(stsymbuffer[count - 1] == '\n')
    stsymbuffer[count - 1] = '\0';

  if(stsymbuffer[0] == '0' && stsymbuffer[1] == 'x') {
    type = ADDRESS;
    sscanf(stsymbuffer, "0x%lx", &address);

    printk("Address is %lx\n", address);

    if(address == 0) {
      printk("Sepextract: Malformed address %s, must be on the form 0xA1B2C3D4\n", stsymbuffer);
      type = STERROR;
      return -1;
    }
  }
  else
    type = SYMBOL;

  return count;
}

int read_symbol(char *buf, char **start, off_t offset, int count, int *eof ) {
  int len = 0;

  if(offset > 0) {
    return 0;
  }

  /* Perform lookup or reverse lookup based on input type
     given by user */
  if(type == ADDRESS) {
    len += sprint_symbol(buf + len, address);
  }
  else if (type == SYMBOL) {
    address = kallsyms_lookup_name(stsymbuffer);
    len += sprintf(buf + len, "%lx", address);
    printk("For symbol %s, found address %lx\n", stsymbuffer, address);
  } else
    len += sprintf(buf + len, "0");
  
  /* If our data length is smaller than what
     the application requested, mark End-Of-File */
  if( len <= count + offset )
    *eof = 1;    /* Initialize the start pointer */

  *start = buf + offset; 

  /* Reduce the offset from the total length */
  len -= offset;
  
  /* Normalize the len variable */
  if( len > count )
    len = count; 

  if( len < 0 )
    len = 0;
    
  return len;
}

void initialize_st() {
  /* Create a proc file for outgoing packets */
  st_trace = create_proc_entry("st",
				 S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH,
				 NULL); 
  if (st_trace)
    {
      /* Setup the Read and Write functions */
      st_trace->read_proc  = (read_proc_t *) &read_symbol;
      st_trace->write_proc = (write_proc_t *) &set_address;
    }
}

void cleanup_st() {
  /* Remove proc entry */
  remove_proc_entry( "st", NULL );
}
