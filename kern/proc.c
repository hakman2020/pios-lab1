/*
 * PIOS process management.
 *
 * Copyright (C) 2010 Yale University.
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Primary author: Bryan Ford
 */

#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <kern/cpu.h>
#include <kern/mem.h>
#include <kern/trap.h>
#include <kern/proc.h>
#include <kern/init.h>
#include <kern/ready_queue.h>



proc proc_null;		// null process - just leave it initialized to 0

proc *proc_root;	// root process, once it's created in init()

// LAB 2: insert your scheduling data structure declarations here.
ready_queue redi_ku;		//process ready queue



void
proc_init(void)
{
	

	if (!cpu_onboot())
		return;

	// your module initialization code here
	ready_queue_init(&redi_ku);
	proc_root = proc_alloc(0,0);
}

// Allocate and initialize a new proc as child 'cn' of parent 'p'.
// Returns NULL if no physical memory available.
proc *
proc_alloc(proc *p, uint32_t cn)
{
	pageinfo *pi = mem_alloc();
	if (!pi)
		return NULL;
	mem_incref(pi);

	proc *cp = (proc*)mem_pi2ptr(pi);
	memset(cp, 0, sizeof(proc));
	spinlock_init(&cp->lock);
	cp->parent = p;
	cp->state = PROC_STOP;

	// Integer register state
	cp->sv.tf.ds = CPU_GDT_UDATA | 3;
	cp->sv.tf.es = CPU_GDT_UDATA | 3;
	cp->sv.tf.cs = CPU_GDT_UCODE | 3;
	cp->sv.tf.ss = CPU_GDT_UDATA | 3;


	if (p)
		p->child[cn] = cp;
	return cp;
}



// Put process p in the ready state and add it to the ready queue.
void
proc_ready(proc *p)
{
	//panic("proc_ready not implemented");
	proc_mark(p, PROC_READY);
	ready_queue_append(&redi_ku, p);
}

#define INT_OPCODE_LEN 2

// Save the current process's state before switching to another process.
// Copies trapframe 'tf' into the proc struct,
// and saves any other relevant state such as FPU state.
// The 'entry' parameter is one of:
//	-1	if we entered the kernel via a trap before executing an insn
//	0	if we entered via a syscall and must abort/rollback the syscall
//	1	if we entered via a syscall and are completing the syscall
void
proc_save(proc *p, trapframe *tf, int entry)
{
	memmove(&p->sv.tf, tf, sizeof(trapframe));
	
	if (entry != PROC_SYSCALL_COMPLETE) {
		char *eip = (char*) p->sv.tf.eip;
		eip -= INT_OPCODE_LEN;
		p->sv.tf.eip = (uintptr_t) eip;
	}
}


// Go to sleep waiting for a given child process to finish running.
// Parent process 'p' must be running and locked on entry.
// The supplied trapframe represents p's register state on syscall entry.
void gcc_noreturn
proc_wait(proc *parent, proc *child, trapframe *parent_tf)
{
	//panic("proc_wait not implemented");
	if (parent->state != PROC_RUN) {
		trap_return(parent_tf);

	}

	proc_mark(parent, PROC_WAIT);
	parent->waitchild = child;
	proc_save(parent, parent_tf, PROC_SYSCALL_RESTART);

	// return to child indirectly thru ready queue
	// for a child to run, it must go thru the ready queue
	proc_sched();
}




void gcc_noreturn
proc_sched(void)
{
	//panic("proc_sched not implemented");
	for (;;) {
		proc *p = ready_queue_pop(&redi_ku);
		if (p) {
			proc_run(p);
		}
		pause();
	}
}



// Switch to and run a specified process, which must already be locked.
void gcc_noreturn
proc_run(proc *p)
{
	//panic("proc_run not implemented");
	proc_mark(p, PROC_RUN);
	trap_return(&p->sv.tf);
}


// Yield the current CPU to another ready process.
// Called while handling a timer interrupt.
void gcc_noreturn
proc_yield(trapframe *tf)
{
	//panic("proc_yield not implemented");

	proc *run_now = ready_queue_pop(&redi_ku);
	
	if (run_now) {
		proc *run_later = proc_cur();
		//cprintf("proc_yield: proc_cur: state=%d\n",run_later->state);
		proc_save(run_later, tf, PROC_SYSCALL_COMPLETE);
		proc_ready(run_later);
		proc_run(run_now);
	}

	trap_return(tf);
}



// Put the current process to sleep by "returning" to its parent process.
// Used both when a process calls the SYS_RET system call explicitly,
// and when a process causes an unhandled trap in user mode.
// The 'entry' parameter is as in proc_save().
void gcc_noreturn
proc_ret(trapframe *tf, int entry)
{
	//panic("proc_ret not implemented");

	proc *child = proc_cur();
	proc *parent = child->parent;

	//cprintf("(%d) proc_ret: parent: state=%d waitchild=%p\n",cpu_cur()->id,parent->state,parent->waitchild);
	
	if (parent->waitchild == child) {
		parent->waitchild = NULL;
		proc_save(child, tf, entry);
		proc_mark(child, PROC_STOP);
		proc_run(parent);
		}

	trap_return(tf);
}



void
proc_mark(proc *p, proc_state state)
{
	p->state = state;
	p->runcpu = NULL;
	if (state == PROC_RUN) {
		p->runcpu = cpu_cur();
		p->runcpu->proc = p;
	}
}


// Helper functions for proc_check()
static void child(int n);
static void child_original(int n);
static void grandchild(int n);

static struct procstate child_state;
static char gcc_aligned(16) child_stack[4][PAGESIZE];

static volatile uint32_t pingpong = 0;
static void *recovargs;



void
proc_check(void)
{
	// Spawn 2 child processes, executing on statically allocated stacks.

	int i;
	for (i = 0; i < 4; i++) {
		// Setup register state for child
		uint32_t *esp = (uint32_t*) &child_stack[i][PAGESIZE];
		*--esp = i;	// push argument to child() function
		*--esp = 0;	// fake return address
		child_state.tf.eip = (uint32_t) child;
		child_state.tf.esp = (uint32_t) esp;
		child_state.tf.cs = (uint32_t) CPU_GDT_UCODE+3;
		child_state.tf.ss = (uint32_t) CPU_GDT_UDATA+3;

		// Use PUT syscall to create each child,
		// but only start the first 2 children for now.
		cprintf("spawning child %d\n", i);
		sys_put(SYS_REGS | (i < 2 ? SYS_START : 0), i, &child_state,
			NULL, NULL, 0);
	}

	// Wait for both children to complete.
	// This should complete without preemptive scheduling
	// when we're running on a 2-processor machine.
	for (i = 0; i < 2; i++) {
		cprintf("waiting for child %d\n", i);
		sys_get(SYS_REGS, i, &child_state, NULL, NULL, 0);
	}
	cprintf("proc_check() 2-child test succeeded\n");

	// (Re)start all four children, and wait for them.
	// This will require preemptive scheduling to complete
	// if we have less than 4 CPUs.
	cprintf("proc_check: spawning 4 children\n");
	for (i = 0; i < 4; i++) {
		cprintf("spawning child %d\n", i);
		sys_put(SYS_START, i, NULL, NULL, NULL, 0);
	}

	// Wait for all 4 children to complete.
	for (i = 0; i < 4; i++)
		sys_get(0, i, NULL, NULL, NULL, 0);
	cprintf("proc_check() 4-child test succeeded\n");

	// Now do a trap handling test using all 4 children -
	// but they'll _think_ they're all child 0!
	// (We'll lose the register state of the other children.)
	i = 0;
	sys_get(SYS_REGS, i, &child_state, NULL, NULL, 0);
		// get child 0's state
	assert(recovargs == NULL);
	do {
		sys_put(SYS_REGS | SYS_START, i, &child_state, NULL, NULL, 0);
		sys_get(SYS_REGS, i, &child_state, NULL, NULL, 0);
		if (recovargs) {	// trap recovery needed
			trap_check_args *args = recovargs;
			cprintf("recover from trap %d\n",
				child_state.tf.trapno);
			child_state.tf.eip = (uint32_t) args->reip;
			args->trapno = child_state.tf.trapno;
		} else
			assert(child_state.tf.trapno == T_SYSCALL);
		i = (i+1) % 4;	// rotate to next child proc
	} while (child_state.tf.trapno != T_SYSCALL);
	assert(recovargs == NULL);

	cprintf("proc_check() trap reflection test succeeded\n");

	cprintf("proc_check() succeeded!\n");
}







static void child(int n)
{
	// Only first 2 children participate in first pingpong test
	if (n < 2) {
		int i;
		for (i = 0; i < 10; i++) {
			cprintf("in child=%d count=%d pingpong=%d\n", n, i, pingpong);
			while (pingpong != n) {
				pause();
			}
			xchg(&pingpong, !pingpong);
		}
		sys_ret();
	}

	// Second test, round-robin pingpong between all 4 children
	int i;
	for (i = 0; i < 10; i++) {
		cprintf("in child %d count %d\n", n, i);
		while (pingpong != n)
			pause();
		xchg(&pingpong, (pingpong + 1) % 4);
	}
	sys_ret();

	// Only "child 0" (or the proc that thinks it's child 0), trap check...
	if (n == 0) {
		assert(recovargs == NULL);
		trap_check(&recovargs);
		assert(recovargs == NULL);
		sys_ret();
	}

	panic("child(): shouldn't have gotten here");
}

static void grandchild(int n)
{
	panic("grandchild(): shouldn't have gotten here");
}

