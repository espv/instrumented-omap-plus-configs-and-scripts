#include <linux/kprobes.h>
#include "sepext_trace.h"
#include <asm/atomic.h>

/* Allocate space to store data */
unsigned int avail_entries;
struct sepentry *entries;
unsigned int lastread;
unsigned int tracing_active;

struct sepext_stats stats = {
  .entrycnt = 0,
  .ctxcnt = 0,
  .touscnt = 0,
  .debug = 0,
};

