#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "LS-to-ns3.h"
#include "ns3/callback.h"
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/node.h"
#include "ns3/cosim.h"
#include "ns3/ptr.h"
#include "ns3/taskscheduler.h"
#include "ns3/event-impl.h"
#include "ns3/execenv.h"
#include "ns3/hwmodel.h"
#include "ns3/interrupt-controller.h"

extern "C" {
	#include "ns3-wrappers.h"
	extern void (*lsRun)(void);
	extern void run_event_handler(void);
}

#define TIMER_INTERRUPT_NUMBER 1

ns3::Ptr<ns3::Node> active_node;
int ticks_enabled = 1;
int idle_pid = 2;

void
set_active_node(ns3::Ptr<ns3::Node> node)
{
  // Set the node so that any timer events
  // scheduled by the LinSched uses the
  // timer programming function of this node.
  active_node = node;
  lsRun = &run_event_handler;
}

int pid_after;

unsigned long
ns3_preempt(int pid) {
    // TODO OYSTEDAL: Add cpu param
    NS_ASSERT(0); // OYSTEDAL: Not updated for multicore
	active_node->GetObject<ns3::ExecEnv> ()->hwModel->cpus[0]->taskScheduler->PreEmpt(0, pid);
	return get_timeof_next_ns3_event();
}

// STEP 3: This is executed once it is due in the interrupt controller
void
executeLinSchedEvent(ns3::Ptr<ns3::Node> node, void (*f)(void *data), void *data)
{
  NS_ASSERT(0); // OYSTEDAL: Not updated for multicore
  int pid_before = get_cr_pid();
  //std::cout << pid_before << " " << ns3::Simulator::Now() << std::endl;

  f(data);

  int pid_after = get_cr_pid();
  //std::cout << pid_after << " " << ns3::Simulator::Now() << std::endl;

  // If we have a new current running, notify the scheduler on the
  // node in question.
  if(pid_after != pid_before)
    node->GetObject<ns3::ExecEnv> ()->hwModel->cpus[0]->taskScheduler->PreEmpt(0, pid_after); // TODO: 
}

// STEP 2: This is executed by the ns-3 Scheduler to model the timer firing off
void
LinSchedInterrupt(ns3::Ptr<ns3::Node> node, void (*f)(void *data), void *data)
{
	ns3::EventImpl *callback = MakeEvent(&executeLinSchedEvent, node, f, data);
	node->GetObject<ns3::ExecEnv> ()->hwModel->m_interruptController->IssueInterruptNoProcessing(TIMER_INTERRUPT_NUMBER, callback);
}

// STEP 1: This is executed by LinSched directly to schedule scheduler ticks
void
schedule_ns3_event(unsigned long time, void (*f)(void *data), void *data)
{
  #ifdef __NS3_COSIM_DEBUG__
  std::cout << __FUNCTION__ << "(" << time << ", " << f << ", " << data << ")" << std::endl;
  #endif

//  if(get_cr_pid() != 2)
	  ns3::Simulator::Schedule (ns3::NanoSeconds(time), &LinSchedInterrupt, active_node, f, data);
}

unsigned long
get_timeof_next_ns3_event(void)
{
	return ns3::Simulator::Next().GetNanoSeconds();
}

unsigned long
get_ns3_time(void)
{
  #ifdef __NS3_COSIM_DEBUG__
  std::cout << __FUNCTION__ << std::endl;
  #endif

  return ns3::Simulator::Now().GetNanoSeconds();
}
