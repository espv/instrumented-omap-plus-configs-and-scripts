# John Calandrino - LinSched Makefile
BASIC_PROGS = linsched fractional_cpu_test 
TESTS = basic_tests batch_balance_test

CC = gcc
LINUXDIR = ..
CFLAGS = -g -m64 -D__KERNEL__ -D__LINSCHED__ -Wall -Wundef -Wstrict-prototypes -Werror-implicit-function-declaration -fno-common -I ${LINUXDIR}/include -I ${LINUXDIR}/arch/x86/include  -include ${LINUXDIR}/include/linux/autoconf.h -include linux_linsched.h -Wno-pointer-sign
CLEANFILES = *.o *.out *~ *.er
OBJFILES = linux_linsched.o numa.o hrtimer.o stubs.o ${LINUXDIR}/kernel/notifier.o ${LINUXDIR}/kernel/timer.o ${LINUXDIR}/kernel/time/clockevents.o ${LINUXDIR}/kernel/sched.o ${LINUXDIR}/kernel/fork.o ${LINUXDIR}/arch/x86/kernel/init_task.o ${LINUXDIR}/kernel/exit.o ${LINUXDIR}/kernel/time.o ${LINUXDIR}/kernel/cpu.o ${LINUXDIR}/mm/percpu.o ${LINUXDIR}/kernel/sched_clock.o ${LINUXDIR}/kernel/sched_cpupri.o ${LINUXDIR}/kernel/hrtimer.o ${LINUXDIR}/kernel/cred.o ${LINUXDIR}/kernel/signal.o ${LINUXDIR}/kernel/pid.o ${LINUXDIR}/kernel/time/timekeeping.o ${LINUXDIR}/kernel/time/ntp.o ${LINUXDIR}/kernel/time/jiffies.o ${LINUXDIR}/kernel/time/tick-common.o ${LINUXDIR}/kernel/time/tick-oneshot.o ${LINUXDIR}/kernel/time/tick-sched.o ${LINUXDIR}/kernel/time/tick-broadcast.o ${LINUXDIR}/init/main.o ${LINUXDIR}/lib/rbtree.o ${LINUXDIR}/lib/div64.o ${LINUXDIR}/lib/cpumask.o ${LINUXDIR}/lib/bitmap.o ${LINUXDIR}/lib/list_debug.o ${LINUXDIR}/lib/find_next_bit.o ${LINUXDIR}/lib/hweight.o ${LINUXDIR}/lib/reciprocal_div.o ${LINUXDIR}/lib/plist.o ${LINUXDIR}/lib/vsprintf.o ${LINUXDIR}/lib/ctype.o ${LINUXDIR}/kernel/semaphore.o ns3-wrappers.o

TEST_LIB = test_lib.o

.PHONY: run_all_tests all

all:		${BASIC_PROGS} ${TESTS}

run_all_tests: ${TESTS}
#make seems to remove stack size ulimits for no apparent reason,
#and stack overflow due to recursion is a plausible bug
	( ulimit -s 8192; ./run_tests.sh $^ )

.SECONDEXPANSION:
${BASIC_PROGS} ${TESTS}: ${OBJFILES} $$@.o
	${CC} ${LFLAGS} -o $@ $^

${TESTS}: ${TEST_LIB}

%.o:		%.c
		${CC} -o $*.o ${CFLAGS} -c $*.c

clean:
		rm -f ${PROGS} ${OBJFILES} ${CLEANFILES} ${ERRORFILE} ${OUTFILE} ${LINSCHED} ${FRACTIONAL_CPU_TEST}

bkup:
		tar cvjf linsched_${LINUXDIR}_bkup.bz2 *
