#ifndef PIOS_KERN_READYQUEUE_H
#define PIOS_KERN_READYQUEUE_H
#ifndef PIOS_KERNEL
# error "This is a kernel header; user programs should not #include it"
#endif

#include <kern/proc.h>


typedef struct ready_queue {
proc *head, *tail;		//ready queue
spinlock lock;	//protects the ready queue
proc dummy;	//always at head of ready queue ( assert(head == &dummy) )
} ready_queue;


void ready_queue_init(ready_queue*);
void ready_queue_ready(ready_queue*, proc*);
proc* ready_queue_sched(ready_queue*);

#endif // !PIOS_KERN_READYQUEUE_H
