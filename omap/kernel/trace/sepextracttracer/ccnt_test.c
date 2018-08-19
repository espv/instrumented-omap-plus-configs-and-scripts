#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdint.h>

#include <errno.h>
#include <limits.h>

#include <sys/time.h>

#define	ARMV7_PMNC_MASK		0x3f	 /* Mask for writable bits */
#define ARMV7_CYCLE_COUNTER 1
#define ARMV7_PMNC_E		(1 << 0) /* Enable all counters */
#define ARMV7_PMNC_P		(1 << 1) /* Reset all counters */
#define ARMV7_PMNC_C		(1 << 2) /* Cycle counter reset */

static inline void pmu_write(unsigned long val) {
	val &= ARMV7_PMNC_MASK;
	// isb();
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(val));
}

static inline unsigned long /*int*/ read_ccnt()
{
	unsigned long value = 0;
    asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (value));
	return value;
}

#include <sys/syscall.h>
#include <pthread.h>

#define CPU_SETSIZE 1024
#define __NCPUBITS  (8 * sizeof (unsigned long))
typedef struct
{
   unsigned long __bits[CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

#define CPU_SET(cpu, cpusetp) \
  ((cpusetp)->__bits[(cpu)/__NCPUBITS] |= (1UL << ((cpu) % __NCPUBITS)))
#define CPU_ZERO(cpusetp) \
  memset((cpusetp), 0, sizeof(cpu_set_t))
void setCurrentThreadAffinityMask(cpu_set_t mask)
{
    int err, syscallres;
    pid_t pid = gettid();
    syscallres = syscall(__NR_sched_setaffinity, pid, sizeof(mask), &mask);
    if (syscallres)
    {
        err = errno;
        // LOGE("Error in the syscall setaffinity: mask=%d=0x%x err=%d=0x%x", mask, mask, err, err);
    }
}

int64_t getTimeNsec() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t) now.tv_sec*1000000000LL + now.tv_nsec;
}

// Galaxy Nexus is 1.2GHz = 1,200,000,000
#define CYCLES_PER_SEC 1200000000

int main(int argc, char *argv[]) {
    printf("CCNT TEST START\n");

    // setCurrentThreadAffinityMask(cpu_set);

    unsigned long long lastCycle = 0;
    for(;;) {
        unsigned long long currentCycle = read_ccnt();

        unsigned long long deltaCycles;
        if (currentCycle < lastCycle) {
            printf("Overflow\n");
            deltaCycles = (UINT_MAX - lastCycle) + currentCycle;
        } else {
            deltaCycles = currentCycle - lastCycle;
        }

        double t = (double)deltaCycles / CYCLES_PER_SEC;

        printf("CCNT: %llu - delta: %llu - time: %f\n", currentCycle, deltaCycles, t);
        lastCycle = currentCycle;

        int64_t now = getTimeNsec();
        while (getTimeNsec() - now < 1000000000LL) {}
    }

    return 0;
}
