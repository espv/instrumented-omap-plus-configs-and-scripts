/* The following must be declared to indicate
   that we are in user space, and cannot use
   kernel headers (for now) */

#define USNL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include "sepext_trace.h"

#define TO_OBTAIN 100000
#define MAXSYMS 100
#define MAXTHREADS 1000

struct symbol {
    void *addr;
    char symbol[255];
};

struct symbol symarray[MAXSYMS];
int lastsym = 0;

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

unsigned int incount, outcount, toobtain;
void *srvaddr;

void collectSymbols() {
    int i = 0;
    char line[300];
    char symname[255];
    void *address;

    /* Read IDS files */
    FILE *fr;
    fr = fopen ("./sepext_srvaddr", "rt");  /* open the file for reading */
    while(fgets(line, 300, fr) != NULL)
    {
        /* get a line, up to 80 chars from fr.  done if NULL */
        sscanf (line, "%x %s", &address, symname);
        /* convert the string to a long int */
        symarray[lastsym].addr = address;
        strcpy(symarray[lastsym].symbol, symname);
        lastsym++;
    }
    fclose(fr);  /* close the file prior to exiting the routine */

    /* Print contents of symbol table */
    printf ("Contents of symbol table:\n");
    for (; i < lastsym; i++) {
        printf("0x%x %s\n", symarray[i].addr, symarray[i].symbol);
    }
}

/* Find a symbol in symbol table */
struct symbol *findSymbol(void *address) {
    return 0;
    int i = 0;
    /* Print contents of symbol table */
    for (; i < lastsym; i++) {
        printf("%x %x\n", symarray[i].addr);
        if(symarray[i].addr == address) {
            return &symarray[i];
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // collectSymbols(); // OYSTEDAL: what's this?
    printf("USNL now running\n");

    srvaddr = 0;
    incount = outcount = 0;
    toobtain = 0;

    /* We must have 0 or 2 arguments */
    if(argc != 1) {
        if(argc < 3) {
            printf("Usage %s [<serviceaddress> <count>]\n",
                    argv[0]);
            exit(1);
        }

        printf("%s\n", argv[1]);

        if(!sscanf(argv[1], "%x", &srvaddr)) { // OYSTEDAL: how do we know addr?
            printf("Invalid format on service address\n");
            exit(1);
        }

        if(!sscanf(argv[2], "%d", &toobtain)) {
            printf("Invalid format on service address\n");
            exit(1);
        }
    }

    sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_NICTEST);
    if(sock_fd<0) {
        perror("socket()");
        return -1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    bind(sock_fd, (struct sockaddr*)&src_addr,
            sizeof(src_addr));

    memset(&dest_addr, 0, sizeof(dest_addr));  
    memset(&dest_addr, 0, sizeof(dest_addr));  
    dest_addr.nl_family = AF_NETLINK;  
    dest_addr.nl_pid = 0;   /* For Linux Kernel */  
    dest_addr.nl_groups = 0; /* unicast */  

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(sizeof(struct sepentry))); 
    memset(nlh, 0, NLMSG_SPACE(sizeof(struct sepentry)));  
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(struct sepentry));  
    nlh->nlmsg_pid = getpid();  
    nlh->nlmsg_flags = 0;

    printf("Size of sepentry: %d\n", sizeof(struct sepentry));

    /* Just insert dummy data; we only send these messages
       to activate the function in the kernel that will
       transmit one entry to us. */
    *((unsigned int *) NLMSG_DATA(nlh)) = 0;

    iov.iov_base = (void *)nlh;  
    iov.iov_len = nlh->nlmsg_len;  
    msg.msg_name = (void *)&dest_addr;  
    msg.msg_namelen = sizeof(dest_addr);  
    msg.msg_iov = &iov;  
    msg.msg_iovlen = 1;  

    /* Main loop for obtaining entries from the kernel */  
    struct sepentry *current_entry;
    int curcount = 0, totcount = 0;

    /* Start by activating tracing */
    nlh->nlmsg_pid = getpid();

    printf("PID: %d\n", nlh->nlmsg_pid);

    *((unsigned int *) NLMSG_DATA(nlh)) = START_TRACING;
    sendmsg(sock_fd,&msg,0);

    printf("Opening file /sdcard/trace.out\n");
    FILE *f = fopen("/sdcard/trace.out", "w");
    int inservice = 0;
    while(totcount < TO_OBTAIN) {
        nlh->nlmsg_pid = getpid();
        *((unsigned int *) NLMSG_DATA(nlh)) = REQUEST_ENTRY;

        /* This starts the loop of exchanging messages
           with the kernel to obtain all entries in the
           trace buffer. */
        sendmsg(sock_fd,&msg,0);  
        recvmsg(sock_fd, &msg, 0);

        /* // OYSTEDAL
           printf("Received message\n");
           printf("Data ptr: 0x%x\n", NLMSG_DATA(nlh));
           printf("Data: %d\n", *(unsigned int*)NLMSG_DATA(nlh));
           */

        /*  If we received a STOP_TRACING, sepextract told us that
            it wants us to stop, and so we now tell it that we are
            finished stopping on our side, and that we want
            sepext to start stopping now. In this case, we simply
            break the loop, as we send sepextract STOP_TRACING
            back right after the loop. */
        if(*((unsigned int *) NLMSG_DATA(nlh)) == STOP_TRACING) {
            printf("STOP_TRACING received\n");
            break;
        }

        /* This signifyes that no more data is to be obtained
           from the current buffer, and that the kernel has
           "emptied" the buffer and restarted tracing. */
        if(*((unsigned int *) NLMSG_DATA(nlh)) == END_OF_DATA)
        {
            printf("Got %d entries from kernel (%d, %d)\n", curcount, incount, outcount);
            fprintf(f, "EOD\n");

            /* Sync data to disk to avoid interaction with
               tracer if we have written any data.
               Inform tracer about disk sync completion. */
            if(curcount) {
                fsync (fileno(f));

                nlh->nlmsg_pid = getpid();

                *((unsigned int *) NLMSG_DATA(nlh)) = SYNC_COMPLETE;
                sendmsg(sock_fd,&msg,0);  
            }

            /* Wait to be awakened by kernel (we know this is
               the next entry) */
            recvmsg(sock_fd, &msg, 0);
            printf("Getting entries from kernel:\n");
            inservice = 0;
            curcount = 0;
            continue;
        }

        // OYSTEDAL: recv of wakeup msg caused segfault
        if(*((unsigned int *) NLMSG_DATA(nlh)) == WAKE_UP_USNL) {
            // printf("Gooooood morning, Vietnam!\n");
            continue;
        }


        // printf("DEBUG 2\n");

        /* TODO: Flush to disk! */
        current_entry = (struct sepentry *) NLMSG_DATA(nlh);

#if 0
        printf("%s %hu %u %hu %hu %x ",
                typestrings[current_entry->type - 1],
                current_entry->currpid,
                current_entry->cycles,
                current_entry->pid1,
                current_entry->pid2,
                current_entry->data);

        printf("%x ", current_entry->addr);
        printf("%x\n", current_entry->location);
#endif

        fprintf(f,"%s %u %hu %u %hu %hu %x ",
                typestrings[current_entry->type - 1],
                current_entry->cpu, // OYSTEDAL
                current_entry->currpid,
                current_entry->cycles,
                current_entry->pid1,
                current_entry->pid2,
                current_entry->data);

        fprintf(f, "%x ", current_entry->addr);
        fprintf(f, "%x\n", current_entry->location);

        /* DEPRECATED: We use post-*/
        /* If we encountered a service with a symbol from IDS, we
           print that. If not, print address */
        /* struct symbol *foundSymbol = findSymbol(current_entry->addr); */
        /* if(foundSymbol) */
        /* 	fprintf(f, "%s ", foundSymbol->symbol); */
        /* else */

        // printf("DEBUG 3\n");
        curcount++;

        // OYSTEDAL: TODO: Why do outcount and incount not update?
        // Seems to be related to service address param

        /* Update number of entries and exits from
           service under investigation.

           Note that it does not matter if we have
           too many service entries, as this will
           be corrected by the analyzer. But we
           must make sure not to count returns that
           does not have entries (e.i., due to entry
           not being trace during sync), as those
           will be disregarded in analyzer too,
           resulting in fewer samples than what the
           user specified */
        if(current_entry->type == SERVICEENTRY &&
                srvaddr && 
                current_entry->addr == srvaddr) {
            inservice = 1;
            incount++;
        }
        else if (current_entry->type == SERVICEEXIT &&
                srvaddr &&
                current_entry->addr == srvaddr &&
                inservice){
            outcount++;
            inservice = 0;
        }

        // printf("DEBUG 4\n");

        /* If we have enough samples, break. */
        if(srvaddr && outcount >= toobtain)
            break;

        /* We we did not set the service,
           obtain default number of entries */
        if(!srvaddr && totcount >= TO_OBTAIN) {
            break;
        }
    }

    printf("Exiting loop...\n");

    /* Disable tracing */
    nlh->nlmsg_pid = getpid();
    *((unsigned int *) NLMSG_DATA(nlh)) = STOP_TRACING;
    sendmsg(sock_fd,&msg,0);  

    printf("Got %d entries from kernel (%d, %d)\n", curcount, incount, outcount);

    close(sock_fd);
    fclose(f);

    return 0;
}
