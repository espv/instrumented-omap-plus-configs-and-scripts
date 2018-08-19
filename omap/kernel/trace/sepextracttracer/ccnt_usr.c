#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <asm/uaccess.h>	/* for copy_from_user */

#define CCNT_US_PROC "ccnt_us"
#define CCNT_RS_PROC "ccnt_rs"
#define CCNT_START_PROC "ccnt_start"

static struct proc_dir_entry *ccnt_us_proc;
static struct proc_dir_entry *ccnt_rs_proc;
static struct proc_dir_entry *ccnt_start_proc;

#define	ARMV7_PMNC_MASK		0x3f	 /* Mask for writable bits */
#define ARMV7_CYCLE_COUNTER 1
#define ARMV7_PMNC_E		(1 << 0) /* Enable all counters */
#define ARMV7_PMNC_P		(1 << 1) /* Reset all counters */
#define ARMV7_PMNC_C		(1 << 2) /* Cycle counter reset */

#define ARMV7_CCNT		31	/* Cycle counter */
#define ARMV7_CNTENS_C		(1 << ARMV7_CCNT)

static inline unsigned long pmu_read(void)
{
	u32 val;
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
	return val;
}

static inline void pmu_write(unsigned long val) {
	val &= ARMV7_PMNC_MASK;
	// isb();
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(val));
}

u32 ccnt_us_read(void) {
    u32 value;
    asm volatile("mrc     p15, 0, r0, c9, c14, 0");
    asm volatile("mov %0, r0":"=r"(value));
    return value;
}

void ccnt_us_set(void) {
    asm volatile("mrc     p15, 0, r0, c9, c14, 0");
    asm volatile("ORR     r0, r0, #0x01");
    asm volatile("mcr     p15, 0, r0, c9, c14, 0");
}

void ccnt_us(void* ignore) {
    // Enable user-mode access
    u32 value = ccnt_us_read();
    ccnt_us_set();
    u32 valuePost = ccnt_us_read();
    printk("      TESTCCNT: Prev value: %x\n", value);
    printk("      TEST CCNT: New value: %x\n", valuePost);

    if (value != 1 && value == valuePost) {
        printk("Failed to set CCNT US bit!\n");
    }
}

void ccnt_reset(void* ignore) {
    pmu_write(ARMV7_PMNC_P | ARMV7_PMNC_C);
    // int32_t value = 1 | 2 | 4 | 8 | 16; // Enables division by 64 as well
    // pmu_write(value);
}

void ccnt_start(void* ignore) {
    // Enable counter
    u32 val = ARMV7_CNTENS_C;
    asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (val));

    // Start counter
	pmu_write(pmu_read() | ARMV7_PMNC_E);
}

int 
ccnt_us_readproc(char *buffer,
	      char **buffer_location,
	      off_t offset, int buffer_length, int *eof, void *data)
{
	int ret;
	
	printk(KERN_INFO "procfile_read (/proc/%s) called\n", CCNT_US_PROC);
	
	if (offset > 0) {
		/* we have finished to read, return 0 */
		ret  = 0;
	} else {
		/* fill the buffer, return the buffer size */
        u32 v = ccnt_us_read();
        printk("Got value: %u\n", v);
        char buf[16];
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%d\n", v);
        memcpy(buffer, buf, sizeof(buf));
        ret = sizeof(buf);
	}

	return ret;
}

int ccnt_us_writeproc(struct file *file, const char *buffer, unsigned long count,
		   void *data)
{
    printk(KERN_INFO "procfile_write (/proc/%s) called\n", CCNT_US_PROC);
    on_each_cpu(ccnt_us, NULL, 1);
    return count;
}

int 
ccnt_rs_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int buffer_length, int *eof, void *data)
{
	int ret = 0;
	
	printk(KERN_INFO "procfile_read (/proc/%s) called\n", CCNT_RS_PROC);

	return ret;
}

int ccnt_rs_write(struct file *file, const char *buffer, unsigned long count,
		   void *data)
{
    printk(KERN_INFO "procfile_write (/proc/%s) called\n", CCNT_RS_PROC);

    on_each_cpu(ccnt_reset, NULL, 1);
    return count;
}

int 
ccnt_start_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int buffer_length, int *eof, void *data)
{
	int ret = 0;
	printk(KERN_INFO "procfile_read (/proc/%s) called\n", CCNT_START_PROC);
	return ret;
}

int ccnt_start_write(struct file *file, const char *buffer, unsigned long count,
		   void *data)
{
    printk(KERN_INFO "procfile_write (/proc/%s) called\n", CCNT_START_PROC);
    on_each_cpu(ccnt_start, NULL, 1);
    return count;
}


static int __init ccnt_usr_init(void)
{
    printk(">>> CCNT USERSPACE INIT\n");

    printk("Enabling userspace access\n");
    on_each_cpu(ccnt_us, NULL, 1);

    ccnt_us_proc = create_proc_entry(CCNT_US_PROC, 0644, NULL);
    if (ccnt_us_proc == NULL) {
        // remove_proc_entry(CCNT_US_PROC, &proc_root);
        printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
                CCNT_US_PROC);
        return -ENOMEM;
    }

	ccnt_us_proc->read_proc  = ccnt_us_readproc;
	ccnt_us_proc->write_proc = ccnt_us_writeproc;
	ccnt_us_proc->mode 	  = S_IFREG | S_IRUGO;
	ccnt_us_proc->uid 	  = 0;
	ccnt_us_proc->gid 	  = 0;
	ccnt_us_proc->size 	  = 37;

    ccnt_rs_proc = create_proc_entry(CCNT_RS_PROC, 0644, NULL);
    if (ccnt_rs_proc == NULL) {
        // remove_proc_entry(CCNT_US_PROC, &proc_root);
        printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
                CCNT_RS_PROC);
        return -ENOMEM;
    }

	ccnt_rs_proc->read_proc  = ccnt_rs_read;
	ccnt_rs_proc->write_proc = ccnt_rs_write;
	ccnt_rs_proc->mode 	  = S_IFREG | S_IRUGO;
	ccnt_rs_proc->uid 	  = 0;
	ccnt_rs_proc->gid 	  = 0;
	ccnt_rs_proc->size 	  = 37;

    ccnt_start_proc = create_proc_entry(CCNT_START_PROC, 0644, NULL);
    if (ccnt_start_proc == NULL) {
        // remove_proc_entry(CCNT_US_PROC, &proc_root);
        printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
                CCNT_START_PROC);
        return -ENOMEM;
    }

	ccnt_start_proc->read_proc  = ccnt_start_read;
	ccnt_start_proc->write_proc = ccnt_start_write;
	ccnt_start_proc->mode 	  = S_IFREG | S_IRUGO;
	ccnt_start_proc->uid 	  = 0;
	ccnt_start_proc->gid 	  = 0;
	ccnt_start_proc->size 	  = 37;

    return 0;
}

void ccnt_usr_exit(void)
{
}

module_init(ccnt_usr_init);
module_exit(ccnt_usr_exit);

MODULE_LICENSE("GPL");

