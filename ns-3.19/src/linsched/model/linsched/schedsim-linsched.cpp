#include "ns3/program.h"
#include "schedsim-linsched.h"
#include "cosim.h"
#include "ns3/thread.h"
#include "ns3/peu.h"
#include "ns3/hwmodel.h"

extern "C" {
#include "ns3/ns3-wrappers.h"
}

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LinSched");
NS_OBJECT_ENSURE_REGISTERED (LinSched);

void testfunc() {
}

TypeId
LinSched::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::processing::linsched")
    .SetParent<TaskScheduler> ()
    .AddConstructor<LinSched> ()
  ;
  return tid;
}

LinSched::LinSched() {
}

int
LinSched::DoRequest(int type, std::vector<uint32_t> arguments)
{
  // We have currently implemented two types of requests;
  // Sleep and Awake. This is the minimal set required to
  // get things running.
  switch(type) {
  case AWAKE:
	  awake(this->threads[arguments[0]]);
	break;
  case SLEEP:
	  goto_sleep();
	break;
  }

  return get_cr_pid();
}

uint32_t
LinSched::DoGetSynchReqType(std::string name)
{
  if(!name.compare("SEMUP"))
    return SEM_UP;
  else if(!name.compare("SEMDOWN"))
    return SEM_DOWN;
  else if(!name.compare("WAITCOMPL"))
    return WAIT_COMPL;
  else if(!name.compare("COMPL"))
    return COMPL;

  return 0;
}


void
LinSched::DoTerminate(void)
{
  exit_thread();
}

void *
LinSched::DoAllocateTempSynch(int type, std::vector<uint32_t> arguments)
{
  switch(type) {
  case SEMAPHORE:
    return new_semaphore(arguments[0]);
    break;
  case COMPLETION:
    return new_completion();
    break;
  }

  return 0;
}

void LinSched::DoDeallocateTempSynch(void* var) {
	free(var);
}

int LinSched::DoTempSynchRequest(int type, void* var, std::vector<uint32_t> arguments){
	  set_time(Simulator::Now().GetNanoSeconds());

	  switch(type) {
	  case SEM_UP:
	    semaphore_up(var);
	    break;
	  case SEM_DOWN:
	    semaphore_down(var);
	    break;
	  case WAIT_COMPL:
	    completion_wait(var);
	    break;
	  case COMPL:
	    completion_complete(var);
	    break;
	  }

	  // We return the prospectively new current
	  // running.
	  // TaskScheduler will perform the necessary
	  // operations to ensure proper task switch
	  // in NetSim space.
	  return get_cr_pid();
}

void
LinSched::DoAllocateSynch(int type, std::string id, std::vector<uint32_t> arguments)
{
  switch(type) {
  case SEMAPHORE:
    semaphores[id] = new_semaphore(arguments[0]);
    break;
  case COMPLETION:
    completions[id] = new_completion();
    break;
  }
}

int
LinSched::DoSynchRequest(int type, std::string id, std::vector<uint32_t> arguments)
{
  set_time(Simulator::Now().GetNanoSeconds());

  switch(type) {
  case SEM_UP:
    semaphore_up(semaphores[id]);
    break;
  case SEM_DOWN:
    semaphore_down(semaphores[id]);
    break;
  case WAIT_COMPL:
    completion_wait(completions[id]);
    break;
  case COMPL:
    completion_complete(completions[id]);
    break;
  }

  // We return the prospectively new current
  // running.
  // TaskScheduler will perform the necessary
  // operations to ensure proper task switch
  // in NetSim space.
 return get_cr_pid();
}

/*
int
LinSched::DoCurrentRunning(void)
{
  return get_cr_pid();
}
*/

void
LinSched::DoInitialize()
{
  // Inform the wrappers that this is the node to use
  set_active_node(peu->hwModel->node);

  // Initialize linux scheduler
  initialize_linsched();
}

void
LinSched::DoHandleSchedulerEvent()
{
  return;
}

int
LinSched::DoFork(int priority)
{
	int pid = 0;
  void *newThread = create_thread(priority, &pid);
  this->threads[pid] = newThread;
  return pid;
}

} // namespace ns3
