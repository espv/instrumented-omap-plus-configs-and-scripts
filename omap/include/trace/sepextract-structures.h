#ifndef __TP_SEPEXTRACT_STRUCTURES_H
#define __TP_SEPEXTRACT_STRUCTURES_H

/* Structures for queues */
#define MAX_QUEUES 100
#define MAX_SYNCH 50
#define MAX_STATE 100

/* Queues, that can be used for packet extraction */
enum sem_queue_id {
  /* NOLOC is used when no queue is associated,
     e.i., either when we are using a general purpose
     queue mechanism and the address of the head
     identifies the queue, or just to extract packet
     information used for filtering during trace
     analysis. */
  PKTQ_NOLOC = 0,
  PKTQ_IP_BACKLOG,
  PKTQ_BCM4329_TXQ,
  PKTQ_WL1251_TXQ,
  PKTQ_NIC_RX,
  PKTQ_NIC_TX,
  PKTQ_NIC, // OYSTEDAL: A combination of the above? 

  PKTQ_DMA_READYCMD1,
  PKTQ_DMA_READYCMD2,
  PKTQ_DMA_READYCMD3,
  PKTQ_DMA_READYCMD4,
  PKTQ_DMA_READYCMD5,
  PKTQ_DMA_READYCMD6,
  PKTQ_DMA_READYCMD7,
  PKTQ_DMA_READYCMD8,
  PKTQ_DMA_READYCMD9,
  PKTQ_DMA_READYCMD10,
  PKTQ_DMA_READYCMD11,
  PKTQ_DMA_READYCMD12,
  PKTQ_DMA_READYCMD13,
  PKTQ_DMA_READYCMD14,
  PKTQ_DMA_READYCMD15,
  PKTQ_DMA_READYCMD16,

  PKTQ_QDISK,

  /* Used to separate pakcet queues from service queues
     in loops. */
  LAST_PKTQ,

  /* DEPRECATED: we keep multiple queues after all */
  /*  SRVQ_SOFTIRQ,*/

  /* Softirqs - all have queue size 1
     DEPRECATED: we use one for all above,
     and create a new specified type of
     queue in Ns-3. This is to avoid
     significant changes to the event
     model required to account for the
     loop de-queueing from these queues
     (see notes).
     NOT DEPR. AFTER ALL: The currentl
     solution handles the problem of
     iterating up to, but not beyond
     the last SoftIRQ, and also that of
     handling re-scheduling of the same
     SoftIRQ after going thorugh all
     1-sized queues once. */
  SRVQ_HI_SOFTIRQ,
  SRVQ_TIMER_SOFTIRQ,
  SRVQ_NET_TX_SOFTIRQ,
  SRVQ_NET_RX_SOFTIRQ,
  SRVQ_BLOCK_SOFTIRQ,
  SRVQ_BLOCK_IOPOLL_SOFTIRQ,
  SRVQ_TASKLET_SOFTIRQ,
  SRVQ_SCHED_SOFTIRQ,
  SRVQ_HRTIMER_SOFTIRQ,
  
  /* Tasklet queues - no limit on size */
  SRVQ_TASKLET,
  SRVQ_TASKLET_HI,

  /* HR-Timer - no limit on size? */
  SRVQ_HRTIMER,

  /* OMAP2 MCSPI */
  SRVQ_OMAP2MCSPI,

  /* WL1251 */
  SRVQ_WL1251_HWWORK,

  /* Hardware IRQs */
  SRVQ_HIRQ,

  QUEUE_LASTSRVQ,
  
  /* SPI/MCSPI/DMA */
  STQ_SPISIZES,
  STQ_SPIOPERATIONS,

  QUEUE_LASTREGISTERED,
};

enum sem_queue_type
{
  QT_FIFO,
  QT_PRIO,
  QT_RED,
  QT_BITMAP,
};

#define MAX_STRINGID_SIZE 50

struct sem_queue_struct
{
  enum sem_queue_type type;
  unsigned int size;
  void *head;
  char stringid[MAX_STRINGID_SIZE];
};

/* These are declared in sepextdefs.c */
extern struct sem_queue_struct sem_queues[MAX_QUEUES];

enum sem_queue_operation {
  QUEUE_ENQUEUE = 0,
  QUEUE_DEQUEUE,
};

enum sem_queue_condition {
  QUEUE_NOTEMPTY = 0,
  QUEUE_EMPTY,
};

enum sem_synch_id {
  SYNCH_TEMPORARY = 0, /* Special meaning: created new each time encountered, and
		associated with next enqueue/previous dequeue. */
  SYNCH_DPC,

  SYNCH_MCSPI_RX,

  SYNCH_MCSPI_TX,

  SYNCH_LASTREGISTERED,

  SYNCH_LAST = MAX_SYNCH, 
};

enum sem_synch_type
{
  ST_SEMAPHORE,
  ST_COMPLETION,
};

enum sem_synch_scope {
  SYNCH_LOCAL, /* i.e., on stack */
  SYNCH_GLOBAL,
};

struct sem_synch
{
  enum sem_synch_type type;
  void *variable;
  char stringid[MAX_STRINGID_SIZE];
};

/* For the thread condition */
enum sem_thread_condition {
  THREAD_NOTREADY,
  THREAD_READY,
};

/* Declared in sepextdefs.c */
extern struct sem_synch sem_synchvars[MAX_SYNCH];

enum sem_state_id {
  STATE_IDENTIFYVIADEVICEFILE = 0,

  STATE_WORKPENDING,
  STATE_RESCHEDQDISC,
  STATE_STOPQDISC,
  STATE_READINTERRUPT,
  STATE_INTERRUPT,
  STATE_INTERRUPTENABLED,
  STATE_NUMTXCOMPLETE,
  STATE_IFFORXMIT,
  STATE_SIZEOFNEXTRXFROMNIC,
  STATE_TRIGGERNICTX,

  // OYSTEDAL
  STATE_SINGLE_READ,
  STATE_DATA_AVAILABLE,
  STATE_CTRL_AVAILABLE,

  /* Add identifiers for state variables here */

  STATE_LASTREGISTERED = MAX_STATE,
};

enum sem_state_scope {
  STATE_LOCAL, /* i.e., on stack */
  STATE_GLOBAL,
};

enum sem_state_operation {
  STATE_READ,
  STATE_WRITE,
};

struct sem_state_condition
{

  /* Any other properties of state variables?
     Note that scope is stored as an event attribute. */

  char stringid[MAX_STRINGID_SIZE];
};

/* Declared in sepextdefs.c */
extern struct sem_state_condition sem_statevars[MAX_STATE];

#endif
