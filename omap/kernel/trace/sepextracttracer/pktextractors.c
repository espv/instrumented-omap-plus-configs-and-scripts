#include <linux/skbuff.h>
#include "sepext_trace.h"
//#include <net/mac80211.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <trace/sepextract.h>
#include <net/icmp.h>

/* The packet extractors should have the responsability to
   stop the tracing. This is because the condition determining 
   when to stop is dependent on the experiment, varying in
   complexity. Thus, establishing a general scheme for determining
   the condition on which to stop tracing will infer too high
   a degree of complexity to be useful.

   The packet extractor writer is responsible for managing the
   data structures necessary to identify the stop condition, and
   to call the stop_tracing() when this occurrs.

   TODO: This file should compile as a separate module to avoid the
   necessity to re-compile and install sepextract for each unique
   collection of packet extractors. */

#if 0 // OYSTEDAL
long pktextract_icmp_rcv(struct sk_buff *skb)
{
  struct icmphdr *icmph;

  // NOTE!: With several extracts, it is imporant to use
  // PKTEXTRN for all extracts except the first. This is
  // for the analyser to be able to distinguish between
  // one extract with several entries and several extractsa
  // with one entry.

  icmph = icmp_hdr(skb);
  if(icmph->type == ICMP_ECHO) { 
    trace_sepext_pktextract(ntohs((unsigned short) icmph->un.echo.sequence), 0);
    //    PKTEXTR(ntohs((unsigned short) icmph->un.echo.sequence)); 
  } 

  /* Always end with a call to jprobe_return(). */
  jprobe_return();
  return 0;  /* <- Never reached */
}
EXPORT_SYMBOL(pktextract_icmp_rcv);
#endif
