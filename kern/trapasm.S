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

TRAPHANDLER_NOEC(th_syscall,T_SYSCALL)
TRAPHANDLER_NOEC(th_ltimer,T_LTIMER)
TRAPHANDLER_NOEC(th_spurious,T_IRQ0 + IRQ_SPURIOUS)



/*
 * Lab 1: Your code here for _alltraps
 */
_alltraps:
pushl %ds
pushl %es
pushl %fs
pushl %gs
pushal
movw $(CPU_GDT_KDATA), %ax
movw %ax, %ds
movw %ax, %es
movl $0, %ebp		// terminate flag for trap_return_ below
pushl %esp			// trapframe*
call trap




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



.globl	trap_return_
.type	trap_return_,@function
//.p2align 4, 0x90		/* 16-byte alignment, nop filled */
trap_return_:
/*
 * Lab 1: Your code here for trap_return
 * trap_return(trapframe*)
 */
popl %eax		//return address
popl %eax		//trapframe*

//
//unwind the stack
//
next:
movl (%ebp), %ebx
cmp $0, %ebx		// previous ebp is 0 for trap() frame (see _alltraps)
je attrap
movl %ebx, %ebp		// next frame 
jmp next

attrap:				// ebp is at trap()
movl %ebp, %esp

//
//restore registers
//
movl (%eax),  %edi
movl 4(%eax), %esi
movl 8(%eax), %ebp
						//dont restore esp
movl 16(%eax), %ebx
movl 20(%eax), %edx
movl 24(%eax), %ecx
						//do eax last
movw 32(%eax), %gs
movw 36(%eax), %fs
movw 40(%eax), %es
movw 44(%eax), %ds

//
//set up for iret
//
pushl 72(%eax)			//ss
pushl 68(%eax)			//esp
pushl 64(%eax)			//eflags
pushl 60(%eax)			//cs
pushl 56(%eax)			//eip

movl 28(%eax), %eax
iret


//
//
.globl	enter_user_mode
.type	enter_user_mode,@function
//.p2align 4, 0x90		/* 16-byte alignment, nop filled */
enter_user_mode:
popl %eax		//return address
popl %ebx		//user function (eip)
popl %eax		//user stack (esp)

//needed?
movw $(CPU_GDT_UDATA+3), %cx	//ss (RPL is 3)
movw %cx, %ds
movw %cx, %es
movw %cx, %fs
movw %cx, %gs

pushl $(CPU_GDT_UDATA+3)	//ss (RPL is 3)
pushl %eax					//esp

pushf	//eflags

pushl $(CPU_GDT_UCODE+3)	//cs (RPL is 3)
pushl %ebx					//eip
iret

