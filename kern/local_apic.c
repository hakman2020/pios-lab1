/*
 * LAPIC interrupt handling.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the xv6 instructional operating system from MIT.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/stdio.h>

#include <kern/trap.h>
#include <kern/proc.h>
#include <kern/local_apic.h>
#include <kern/cpu.h>

#include <dev/lapic.h>


static unsigned n = 0;

static void
do_ltimer(trapframe *tf)
{
	if ((n % 25) == 0 || (n % 25) == 1) {
		//cprintf("do_ltimer=%d cpu=%d\n", n, cpu_cur()->id);
	}
	n++;
	lapic_eoi();		//clear interrupt
	proc_yield(tf);
}


static void
do_spurious(trapframe *tf)
{
	cprintf("spurious interrupt: T_IRQ0+IRQ_SPURIOUS\n");
	trap_return(tf);
}



// Common function to handle all LAPIC interrupt handling.

void
local_apic(trapframe *tf)
{
	switch (tf->trapno) {
	case T_LTIMER:	return do_ltimer(tf);
	case T_IRQ0+IRQ_SPURIOUS:	return do_spurious(tf);
	default:	return;		// handle as a regular trap
	}
}

 