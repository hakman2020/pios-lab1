/*
 * Processor trap handling.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/cpu.h>
#include <kern/trap.h>
#include <kern/cons.h>
#include <kern/init.h>
#include <kern/proc.h>
#include <kern/syscall.h>
#include <kern/local_apic.h>

#include <dev/lapic.h>


// Interrupt descriptor table.  Must be built at run time because
// shifted function addresses can't be represented in relocation records.
static struct gatedesc idt[256];

// This "pseudo-descriptor" is needed only by the LIDT instruction,
// to specify both the size and address of th IDT at once.
static struct pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


extern void th_divide(void);
extern void th_debug(void);
extern void th_nmi(void);
extern void th_brkpt(void);
extern void th_oflow(void);
extern void th_bound(void);
extern void th_illop(void);
extern void th_device(void);

extern void th_dblflt(void);
extern void th_tss(void);
extern void th_segnp(void);
extern void th_stack(void);
extern void th_gpflt(void);
extern void th_pgflt(void);

extern void th_fperr(void);

extern void th_align(void);

extern void th_mchk(void);
extern void th_simd(void);
extern void th_secev(void);

extern void th_syscall(void);
extern void th_ltimer(void);
extern void th_spurious(void);


static void
trap_init_idt(void)
{
	extern segdesc gdt[];
	
	//panic("trap_init() not implemented.");

	SETGATE(idt[T_DIVIDE],0,CPU_GDT_KCODE,th_divide,0)
	SETGATE(idt[T_DEBUG],0,CPU_GDT_KCODE,th_debug,0)
	SETGATE(idt[T_NMI],0,CPU_GDT_KCODE,th_nmi,0)
	SETGATE(idt[T_BRKPT],0,CPU_GDT_KCODE,th_brkpt,3)
	SETGATE(idt[T_OFLOW],0,CPU_GDT_KCODE,th_oflow,3)
	SETGATE(idt[T_BOUND],0,CPU_GDT_KCODE,th_bound,0)
	SETGATE(idt[T_ILLOP],0,CPU_GDT_KCODE,th_illop,0)
	SETGATE(idt[T_DEVICE],0,CPU_GDT_KCODE,th_device,0)
	SETGATE(idt[T_DBLFLT],0,CPU_GDT_KCODE,th_dblflt,0)
	SETGATE(idt[T_TSS],0,CPU_GDT_KCODE,th_tss,0)
	SETGATE(idt[T_SEGNP],0,CPU_GDT_KCODE,th_segnp,0)
	SETGATE(idt[T_STACK],0,CPU_GDT_KCODE,th_stack,0)
	SETGATE(idt[T_GPFLT],0,CPU_GDT_KCODE,th_gpflt,0)
	SETGATE(idt[T_PGFLT],0,CPU_GDT_KCODE,th_pgflt,0)
	SETGATE(idt[T_FPERR],0,CPU_GDT_KCODE,th_fperr,0)
	SETGATE(idt[T_ALIGN],0,CPU_GDT_KCODE,th_align,0)
	SETGATE(idt[T_MCHK],0,CPU_GDT_KCODE,th_mchk,0)
	SETGATE(idt[T_SIMD],0,CPU_GDT_KCODE,th_simd,0)
	SETGATE(idt[T_SECEV],0,CPU_GDT_KCODE,th_secev,0)

	SETGATE(idt[T_SYSCALL],0,CPU_GDT_KCODE,th_syscall,3)
	SETGATE(idt[T_LTIMER],0,CPU_GDT_KCODE,th_ltimer,3)
	SETGATE(idt[T_IRQ0 + IRQ_SPURIOUS],0,CPU_GDT_KCODE,th_spurious,3)



}

void
trap_init(void)
{
	// The first time we get called on the bootstrap processor,
	// initialize the IDT.  Other CPUs will share the same IDT.
	if (cpu_onboot())
		trap_init_idt();

	// Load the IDT into this processor's IDT register.
	asm volatile("lidt %0" : : "m" (idt_pd));

	// Check for the correct IDT and trap handler operation.
	if (cpu_onboot())
		//trap_check_kernel();
		;
}

const char *trap_name(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= T_IRQ0 && trapno < T_IRQ0 + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

void
trap_print_regs(pushregs *regs)
{
	cprintf("  edi  0x%08x\n", regs->edi);
	cprintf("  esi  0x%08x\n", regs->esi);
	cprintf("  ebp  0x%08x\n", regs->ebp);
//	cprintf("  oesp 0x%08x\n", regs->oesp);	don't print - useless
	cprintf("  ebx  0x%08x\n", regs->ebx);
	cprintf("  edx  0x%08x\n", regs->edx);
	cprintf("  ecx  0x%08x\n", regs->ecx);
	cprintf("  eax  0x%08x\n", regs->eax);
}

void
trap_print(trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	trap_print_regs(&tf->regs);
	cprintf("  es   0x----%04x\n", tf->es);
	cprintf("  ds   0x----%04x\n", tf->ds);
	cprintf("  trap 0x%08x %s\n", tf->trapno, trap_name(tf->trapno));
	cprintf("  err  0x%08x\n", tf->err);
	cprintf("  eip  0x%08x\n", tf->eip);
	cprintf("  cs   0x----%04x\n", tf->cs);
	cprintf("  flag 0x%08x\n", tf->eflags);
	cprintf("  esp  0x%08x\n", tf->esp);
	cprintf("  ss   0x----%04x\n", tf->ss);
}

void gcc_noreturn
trap(trapframe *tf)
{
	// The user-level environment may have set the DF flag,
	// and some versions of GCC rely on DF being clear.
	asm volatile("cld" ::: "cc");

	// If this trap was anticipated, just use the designated handler.
	cpu *c = cpu_cur();
	if (c->recover)
		c->recover(tf, c->recoverdata);

	// Lab 2: your trap handling code here!
	if (tf->trapno == T_SYSCALL) {
		syscall(tf);
	}

	if (tf->trapno == T_LTIMER) {
		local_apic(tf);
	}

	if (tf->trapno == T_IRQ0+IRQ_SPURIOUS) {
		local_apic(tf);
	}

	if (tf->trapno >= T_DIVIDE && tf->trapno <= T_SECEV) {
		if (tf->cs & 3) {
			proc_ret(tf, PROC_TRAP_REFLECT);
		}
	}

	// If we panic while holding the console lock,
	// release it so we don't get into a recursive panic that way.
	if (spinlock_holding(&cons_lock))
		spinlock_release(&cons_lock);
	trap_print(tf);
	panic("unhandled trap");
}

void gcc_noreturn
trap_return(trapframe *tf)
{
	tf->eflags = tf->eflags | FL_IF;	//interrupt flag for preemption
	trap_return_(tf);
}


// Helper function for trap_check_recover(), below:
// handles "anticipated" traps by simply resuming at a new EIP.
static void gcc_noreturn
trap_check_recover(trapframe *tf, void *recoverdata)
{
	trap_check_args *args = recoverdata;
	tf->eip = (uint32_t) args->reip;	// Use recovery EIP on return
	args->trapno = tf->trapno;		// Return trap number
	trap_return(tf);
}

// Check for correct handling of traps from kernel mode.
// Called on the boot CPU after trap_init() and trap_setup().
void
trap_check_kernel(void)
{
	assert((read_cs() & 3) == 0);	// better be in kernel mode!

	cpu *c = cpu_cur();
	c->recover = trap_check_recover;
	trap_check(&c->recoverdata);
	c->recover = NULL;	// No more mr. nice-guy; traps are real again

	cprintf("trap_check_kernel() succeeded!\n");
}

// Check for correct handling of traps from user mode.
// Called from user() in kern/init.c, only in lab 1.
// We assume the "current cpu" is always the boot cpu;
// this true only because lab 1 doesn't start any other CPUs.
void
trap_check_user(void)
{
	assert((read_cs() & 3) == 3);	// better be in user mode!

	cpu *c = &cpu_boot;	// cpu_cur doesn't work from user mode!
	assert(c->magic == CPU_MAGIC);	//gch
	c->recover = trap_check_recover;
	trap_check(&c->recoverdata);
	c->recover = NULL;	// No more mr. nice-guy; traps are real again

	cprintf("trap_check_user() succeeded!\n");
}

void after_div0();
void after_breakpoint();
void after_overflow();
void after_bound();
void after_illegal();
void after_gpfault();
void after_priv();

// Multi-purpose trap checking function.
void
trap_check(void **argsp)
{
	volatile int cookie = 0xfeedface;
	volatile trap_check_args args;
	*argsp = (void*)&args;	// provide args needed for trap recovery

	// Try a divide by zero trap.
	// Be careful when using && to take the address of a label:
	// some versions of GCC (4.4.2 at least) will incorrectly try to
	// eliminate code it thinks is _only_ reachable via such a pointer.
	args.reip = after_div0;
	asm volatile("div %0,%0; after_div0:" : : "r" (0));
	assert(args.trapno == T_DIVIDE);

	// Make sure we got our correct stack back with us.
	// The asm ensures gcc uses ebp/esp to get the cookie.
	asm volatile("" : : : "eax","ebx","ecx","edx","esi","edi");
	assert(cookie == 0xfeedface);

	// Breakpoint trap
	args.reip = after_breakpoint;
	asm volatile("int3; after_breakpoint:");
	assert(args.trapno == T_BRKPT);

	// Overflow trap
	args.reip = after_overflow;
	asm volatile("addl %0,%0; into; after_overflow:" : : "r" (0x70000000));
	assert(args.trapno == T_OFLOW);

	// Bounds trap
	args.reip = after_bound;
	int bounds[2] = { 1, 3 };
	asm volatile("boundl %0,%1; after_bound:" : : "r" (0), "m" (bounds[0]));
	assert(args.trapno == T_BOUND);

	// Illegal instruction trap
	args.reip = after_illegal;
	asm volatile("ud2; after_illegal:");	// guaranteed to be undefined
	assert(args.trapno == T_ILLOP);

	// General protection fault due to invalid segment load
	args.reip = after_gpfault;
	asm volatile("movl %0,%%fs; after_gpfault:" : : "r" (-1));
	assert(args.trapno == T_GPFLT);

	// General protection fault due to privilege violation
	if (read_cs() & 3) {
		args.reip = after_priv;
		asm volatile("lidt %0; after_priv:" : : "m" (idt_pd));
		assert(args.trapno == T_GPFLT);
	}

	// Make sure our stack cookie is still with us
	assert(cookie == 0xfeedface);

	*argsp = NULL;	// recovery mechanism not needed anymore
}

