#include  <kern/ready_queue.h>
#include  <inc/x86.h>

void
ready_queue_init(ready_queue *q) {
	q->head = q->tail = &q->dummy;
	spinlock_init(&q->lock);
}

void
ready_queue_ready(ready_queue *q, proc *p)
{
	//panic("proc_ready not implemented");

	spinlock_acquire(&q->lock);
	q->tail->readynext = p;		//add to end of queue
	q->tail = p;				//slide on down
	spinlock_release(&q->lock);
}

proc*
ready_queue_sched(ready_queue *q)
{
	//panic("proc_sched not implemented");
	for (;;) {
		spinlock_acquire(&q->lock);
		if (q->head != q->tail) {		//is queue not empty?
			break;
		}
		spinlock_release(&q->lock);
		pause();
	}

	proc *readynext = q->head->readynext;	//first process in queue
	if (readynext == q->tail) {				//is queue length 1?
		q->tail = q->head;					//queue is now empty
	} else {
		q->head->readynext = readynext->readynext;	//remove first process from queue
	}
	
	spinlock_release(&q->lock);
	return readynext;
}