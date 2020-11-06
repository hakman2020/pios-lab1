/*
 * Trap handler entry and exit code, written in assembly language.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/mmu.h>
#include <inc/trap.h>


#include <kern/cpu.h>



/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 */ 
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 1: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(th_divide,T_DIVIDE)
TRAPHANDLER_NOEC(th_debug,T_DEBUG)
TRAPHANDLER_NOEC(th_nmi,T_NMI)
TRAPHANDLER_NOEC(th_brkpt,T_BRKPT)
TRAPHANDLER_NOEC(th_oflow,T_OFLOW)
TRAPHANDLER_NOEC(th_bound,T_BOUND)
TRAPHANDLER_NOEC(th_illop,T_ILLOP)
TRAPHANDLER_NOEC(th_device,T_DEVICE)

TRAPHANDLER(th_dblflt,T_DBLFLT)
TRAPHANDLER(th_tss,T_TSS)
TRAPHANDLER(th_segnp,T_SEGNP)
TRAPHANDLER(th_stack,T_STACK)
TRAPHANDLER(th_gpflt,T_GPFLT)
TRAPHANDLER(th_pgflt,T_PGFLT)

TRAPHANDLER(th_fperr,T_FPERR)

TRAPHANDLER_NOEC(th_align,T_ALIGN)

TRAPHANDLER(th_mchk,T_MCHK)
TRAPHANDLER(th_simd,T_SIMD)
TRAPHANDLER(th_secev,T_SECEV)






/*
 * Lab 1: Your code here for _alltraps
 */
_alltraps:
pushw 0
pushw %ds
pushw 0
pushw %es
pushw 0
pushw %fs
pushw 0
pushw %gs
pushal
movw $(CPU_GDT_KDATA), %ax
movw %ax, %ds
movw %ax, %es
pushl %esp			// trapframe*
call trap           // trap(trapframe*)




//
// Trap return code.
// C code in the kernel will call this function to return from a trap,
// providing the 
// Restore the CPU state from a given trapframe struct
// and return from the trap using the processor's 'iret' instruction.
// This function does not return to the caller,
// since the new CPU state this function loads
// replaces the caller's stack pointer and other registers.
//
.globl	trap_return
.type	trap_return,@function
.p2align 4, 0x90		/* 16-byte alignment, nop filled */
trap_return:
/*
 * Lab 1: Your code here for trap_return
 */
//1:	jmp	1b		// just spin

// trap_return(trapframe *tf)
//movl (%ebp), %eax
//movl %eax, %ebp


// trap_check_recover(trapframe *tf, void *recoverdata)
movl (%ebp), %eax
movl %eax, %ebp

// trap(trapframe *tf)
add $8, %ebp
movl %ebp, %esp

popl %eax	//trapframe*
popal
popw %gs
popw %ax	//0
popw %fs
popw %ax	//0
popw %es
popw %ax	//0
popw %ds
popw %ax	//0
popl %eax	//trap number
popl %eax	//error code
iret



#define FL_IOPL_3	0x00003000	//   IOPL == 3

//
//
.globl	enter_user_mode
.type	enter_user_mode,@function
.p2align 4, 0x90		/* 16-byte alignment, nop filled */
enter_user_mode:
popl %eax					//return address
popl %ebx					//first user function ( (void)(*)(void) )
popl %eax					//user stack (char*)

//needed?
movw $(CPU_GDT_UDATA+3), %cx	//ss (RPL is 3)
movw %cx, %ds
movw %cx, %es
movw %cx, %fs
movw %cx, %gs

pushl $(CPU_GDT_UDATA+3)	//ss (RPL is 3)
pushl %eax					//esp

pushf
popl %eax
movl $(FL_IOPL_3), %ecx		//I/O privilege level set to 3
or %eax, %ecx				//for using cprintf() in user mode
pushl %ecx					//eflags

pushl $(CPU_GDT_UCODE+3)	//cs (RPL is 3)
pushl %ebx					//eip
iret

