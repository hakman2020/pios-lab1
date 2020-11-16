/*
 * System call handling.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the xv6 instructional operating system from MIT.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/x86.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/trap.h>
#include <inc/syscall.h>

#include <kern/cpu.h>
#include <kern/trap.h>
#include <kern/proc.h>
#include <kern/syscall.h>





// This bit mask defines the eflags bits user code is allowed to set.
#define FL_USER		(FL_CF|FL_PF|FL_AF|FL_ZF|FL_SF|FL_DF|FL_OF)


// During a system call, generate a specific processor trap -
// as if the user code's INT 0x30 instruction had caused it -
// and reflect the trap to the parent process as with other traps.
static void gcc_noreturn
systrap(trapframe *utf, int trapno, int err)
{
	panic("systrap() not implemented.");
}

// Recover from a trap that occurs during a copyin or copyout,
// by aborting the system call and reflecting the trap to the parent process,
// behaving as if the user program's INT instruction had caused the trap.
// This uses the 'recover' pointer in the current cpu struct,
// and invokes systrap() above to blame the trap on the user process.
//
// Notes:
// - Be sure the parent gets the correct trapno, err, and eip values.
// - Be sure to release any spinlocks you were holding during the copyin/out.
//
static void gcc_noreturn
sysrecover(trapframe *ktf, void *recoverdata)
{
	panic("sysrecover() not implemented.");
}

// Check a user virtual address block for validity:
// i.e., make sure the complete area specified lies in
// the user address space between VM_USERLO and VM_USERHI.
// If not, abort the syscall by sending a T_GPFLT to the parent,
// again as if the user program's INT instruction was to blame.
//
// Note: Be careful that your arithmetic works correctly
// even if size is very large, e.g., if uva+size wraps around!
//
static void checkva(trapframe *utf, uint32_t uva, size_t size)
{
	panic("checkva() not implemented.");
}

// Copy data to/from user space,
// using checkva() above to validate the address range
// and using sysrecover() to recover from any traps during the copy.
void usercopy(trapframe *utf, bool copyout,
			void *kva, uint32_t uva, size_t size)
{
	checkva(utf, uva, size);

	// Now do the copy, but recover from page faults.
	panic("syscall_usercopy() not implemented.");
}

static void
do_cputs(trapframe *tf, uint32_t cmd)
{
	// Print the string supplied by the user: pointer in EBX
	cprintf("sc->%s", (char*)tf->regs.ebx);

	trap_return(tf);	// syscall completed
	//enter_user_mode((void*)tf->eip, (void*)tf->esp);
}

static void
do_put(trapframe *tf, uint32_t cmd)
{
	uint32_t flags = tf->regs.eax;
	uint32_t child_slot = tf->regs.edx;
	procstate *child_state = (procstate *) tf->regs.ebx;

	cpu_which_where("do_put");
	proc *parent = cpu_cur()->proc;
	assert(parent);

	proc *child = parent->child[child_slot];
	if (!child) {
		child = proc_alloc(parent, child_slot);
	}

	//put parent to sleep and wait for child to call sys_ret() (which will stop it)
	if (child->state != PROC_STOP) {
		// lab 2 part 3: dont worry about multiprocesser now
		// get it working for single processer first
	}

	// the _put_ part of do_put()
	if (flags & SYS_REGS) {
		child->sv.tf.eip = child_state->tf.eip;
		child->sv.tf.esp = child_state->tf.esp;
	}

	// run the child
	if (flags & SYS_START) {
		parent->state = PROC_WAIT;
		proc_run(child);
	}

	// no  _start_ or _put_ requested, so just return
	// apparently, do_put() can be dont_put()
	// ho humm boring. actually not. you may have just created a brand new process.
	// congratulations to the new parent!
	trap_return(tf);
}

static void
do_get(trapframe *tf, uint32_t cmd)
{
	uint32_t flags = tf->regs.eax;
	uint32_t child_slot = tf->regs.edx;
	procstate *save = (procstate *) tf->regs.ebx;

	proc *parent = proc_cur();
	assert(parent);

	proc *child = parent->child[child_slot];
	if (!child) {
		trap_return(tf);
	}

	//put parent to sleep and wait for child to return
	if (child->state != PROC_STOP) {
		parent->state = PROC_WAIT;
		parent->waitchild = child;
		parent->sv.tf.eip = tf->eip;
		parent->sv.tf.esp = tf->esp;
		tf->eip = child->sv.tf.eip;
		tf->esp = child->sv.tf.esp;
		trap_return(tf);
	}

	// the get part of do_get()
	if (flags & SYS_REGS) {
		save->tf.eip = child->sv.tf.eip;
		save->tf.esp = child->sv.tf.esp;
	}

	trap_return(tf);
}

static void
do_ret(trapframe *tf, uint32_t cmd)
{
	proc *child = proc_cur();
	assert(child);
	proc *parent = child->parent;
	assert(parent);

	uint32_t child_slot = tf->regs.edx;
	cprintf("do_ret: child=%d  parent=%x  child=%x", child_slot, parent->waitchild, child);

	if (parent->waitchild == child) {
		child->state = PROC_STOP;
		proc_ready(parent);
		proc_sched();
	}

	trap_return(tf);
}

// Common function to handle all system calls -
// decode the system call type and call an appropriate handler function.
// Be sure to handle undefined system calls appropriately.
void
syscall(trapframe *tf)
{
	// EAX register holds system call command/flags
	uint32_t cmd = tf->regs.eax;
	switch (cmd & SYS_TYPE) {
	case SYS_CPUTS:	return do_cputs(tf, cmd);
	// Your implementations of SYS_PUT, SYS_GET, SYS_RET here...
	case SYS_PUT:	return do_put(tf, cmd);
	case SYS_GET:	return do_get(tf, cmd);
	case SYS_RET:	return do_ret(tf, cmd);
	default:	return;		// handle as a regular trap
	}
}

