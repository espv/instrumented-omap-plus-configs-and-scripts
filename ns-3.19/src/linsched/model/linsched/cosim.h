#ifndef COSIM_H
#define COSIM_H

///////////
// The following functions are called from Ns-3
///////////

#ifdef __cplusplus

#include "ns3/core-module.h"
#include "ns3/node.h"
#include "ns3/ptr.h"

void set_active_node(ns3::Ptr<ns3::Node> node);

#endif

///////////
// The following functions are called from LinSched
// and therefore need to have C linkage
///////////

#ifdef __cplusplus
extern "C" {
#endif

void schedule_ns3_event(unsigned long time, void (*f)(void *data), void *data);
unsigned long get_timeof_next_ns3_event(void);
unsigned long get_ns3_time(void);
unsigned long ns3_preempt(int pid);
int fractional(void);

#ifdef __cplusplus
}
#endif

#endif // COSIM_H
