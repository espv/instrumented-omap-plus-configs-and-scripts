#ifndef SCHEDSIM_LINSCHED_H
#define SCHEDSIM_LINSCHED_H

#include <ns3/taskscheduler.h>
#include <vector>

namespace ns3 {

enum RequestType {
  AWAKE,
  SLEEP
};

enum SynchRequestType {
  SEM_UP,
  SEM_DOWN,
  WAIT_COMPL,
  COMPL
};

enum SynchType {
  SEMAPHORE,
  COMPLETION,
};

class LinSched : public TaskScheduler
{
public:
  LinSched();
  static TypeId GetTypeId(void);

protected:
  // NOT NEEDED?
  //  Ptr<Thread> m_currentRunning;
  //  std::map<int, Ptr<Thread> > m_threads;

  // The functions below must be implemented by a subclass
  // for a specific SchedSim
  virtual void DoInitialize();
  virtual void DoHandleSchedulerEvent();
  virtual int DoFork(int priority);
  virtual void DoTerminate(void);

  // To allocate synch. variables
  virtual void DoAllocateSynch(int type, std::string id, std::vector<uint32_t> arguments);
  void * DoAllocateTempSynch(int type, std::vector<uint32_t> arguments);
  void DoDeallocateTempSynch(void* var);

  // To execute requests
  virtual int DoRequest(int type, std::vector<uint32_t> arguments);
  virtual int DoSynchRequest(int type, std::string id, std::vector<uint32_t> arguments);
  virtual int DoTempSynchRequest(int type, void* var, std::vector<uint32_t> arguments);
  virtual uint32_t DoGetSynchReqType(std::string name);

  // TODO: virtual std::vector<int> DoCurrentRunning(void);

 private:
  // Data structures to store synchronization primitives
  std::map<std::string, void *> semaphores; 
  std::map<std::string, void *> completions;
 // Currently, we're only allowing one node to have a processing model,
  // but this is the class where the necessary global data structures and
  // modified PER_CPU etc., are to be implemented.
  std::map<int, void *> threads;
};

}

#endif
