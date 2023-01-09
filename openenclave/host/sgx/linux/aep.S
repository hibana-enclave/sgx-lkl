// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include "../asmdefs.h"


//==============================================================================
//
// void OE_AEP(void)
//
//     Asynchronous Exception Pointer (AEP) function that handles exceptions
//     and interrupts from an enclave. A pointer to this function is passed
//     to the EENTER instruction and this function. This implementation resumes
//     execution of the enclave (ERESUME).
//
//     This function must not use or modify the stack, else it could overwrite
//     the host stack region used by enclave host stack allocaiton routines.
//
//==============================================================================
OE_AEP:
.cfi_startproc

// N.B, the AEP must be a ERESUME(ENCLU[3]) directly, otherwise single step
// debugging can't work. When an AEX happens, the rax in processor synthentic
// state will be set to 3, and the rbx will be set to TCS of interrupted
// enclave thread automatically. Host side doesn't need to and shouldn't do
// additional settings.

.aep:
    ENCLU              /* ENCLU (like return, the end of procedure) */
    ud2                /* this line is never reached */

.cfi_endproc


//==============================================================================
//
// void SGX_STEP_AEP_IMPL(void)
//
//==============================================================================
SGX_STEP_AEP_IMPL:
.cfi_startproc

.sgx_step_aep_impl_codebase:
   /*
     * Save TCS address thread-local on the stack
     * NOTE: maintain 16-byte stack alignment (ABI calling convention)
     */
    push %rbx
    mov %rsp, %rbp
    and $~0xf, %rsp

    /* optional C function callback */
    lea sgx_step_aep_cb(%rip), %rax
    mov (%rax), %rax
    test %rax, %rax
    je .Leresume
    call *%rax

.Leresume:
    incl sgx_step_eresume_cnt(%rip)

    /* restore stack and TCS address */
    mov %rbp, %rsp
    pop %rbx

    mov SGX_STEP_AEP_ADDRESS(%rip), %rcx   /* AEP address */

    prefetch nemesis_tsc_eresume(%rip)
    mfence

    rdtsc
    mov %eax, nemesis_tsc_eresume(%rip)

    mov $3, %rax                                    /* ERESUME leaf */
    .byte 0x0f, 0x01, 0xd7                          /* ENCLU */
    ud2                                             /* this line is never reached */

.cfi_endproc


//==============================================================================
//
// uint64_t OE_AEP_ADDRESS
//
//     The address of the ENCLU instruction is stored in this variable.
//     If the OE_AEP function were to be used in code, the linker could create
//     thunks that wrap the function. For example, when incremental linking is
//     enabled, the linker on windows creates an entry in the ILT table for
//     each function and uses that wherever the function is referenced.
//     Thus OE_AEP would end up pointing to the thunk in the ILT which is not
//     what we want. The OE_AEP_ADDRESS variable gives the precise location of
//     the ENCLU instruction.
//
//==============================================================================
.globl OE_AEP_ADDRESS
.section .rodata
.align 8
OE_AEP_ADDRESS:	.quad .aep


//==============================================================================
//
// uint64_t SGX_STEP_AEP_ADDRESS
//
//==============================================================================
.globl SGX_STEP_AEP_ADDRESS
.section .rodata
.align 8
SGX_STEP_AEP_ADDRESS:	.quad .sgx_step_aep_impl_codebase


//==============================================================================
//
// SGX-STEP
//
//==============================================================================

// XXX runtime reconfigurable indirect Asynchronous Exit Pointer (AEP)
// (ld complains when initializing __default_async_exit_pointer here, so we have
// to do it at runtime, when EENTERing, below in .Ldo_eenter.

.globl sgx_step_aep_cb
.section .data
sgx_step_aep_cb:
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x0

// XXX HACK: SGX stores TCS address in rbx on interrupt, but this value is
// somehow not properly stored in Linux's pt_regs struct available to our
// driver's interrupt handler. We therefore store TCS address here in the
// untrusted runtime, so as to be able to explicitly communicate TCS to our
// driver...

.globl g_tcs
.section .data
g_tcs:
    .word 0x0
    .word 0x0
    .word 0x0
    .word 0x0

