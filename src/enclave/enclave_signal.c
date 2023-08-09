#include <signal.h>
#include <string.h>

#include <asm/sigcontext.h>

#include <asm-generic/ucontext.h>

#include <lkl_host.h>
#include <lkl/setup.h>
#include <stdio.h>
#include <string.h>

#include <openenclave/enclave.h>
#include <openenclave/internal/cpuid.h>

#include "enclave/enclave_oe.h"
#include "enclave/enclave_util.h"
#include "enclave/lthread.h"
#include "enclave/sgxlkl_t.h"
#include "shared/env.h"

#define RDTSC_OPCODE 0x310F

#define UD2_OPCODE 0xB0F  /* trigger sgx-step attack in enclave 
                                0F 0B    UD2    <https://mudongliang.github.io/x86/html/file_module_x86_id_318.html>
                                0F A2   CPUID   <https://mudongliang.github.io/x86/html/file_module_x86_id_45.html>
                                https://github.com/lsds/openenclave/blob/feature.sgx-lkl/include/openenclave/internal/cpuid.h#L9
                              */ 

// -----------------------------------------------------------------------
// H(x) = (x * 229) mod 677 
static const int sgx_step_attack_signal_hash_int = 229; 
static const int sgx_step_attack_signal_hash_mod = 677;
//static const int sgx_step_attack_signal_hash_key = 3559; // (0xDE7)
static const int sgx_step_attack_signal_hash_target = 580; 
// ------------------------------------------------------------------------

/* Mapping between OE and hardware exception */
struct oe_hw_exception_map
{
    uint32_t oe_code;  /* OE exception code */
    int trapnr;        /* Hardware trap no  */
    int signo;         /* Signal for trap   */
    bool supported;    /* Enabled in SGX-LKL */
    char* description; /* Description string */
};

// clang-format off
/* Encapsulating the exception information to a datastructure.
 * supported field is marked a true/false based on the current
 * support. Once all signal support is enabled from OE, then
 * all entry will be marked as true.
 */
static struct oe_hw_exception_map exception_map[] = {
    {OE_EXCEPTION_DIVIDE_BY_ZERO, X86_TRAP_DE, SIGFPE, true, "SIGFPE (divide by zero)"},
    {OE_EXCEPTION_BREAKPOINT, X86_TRAP_BP, SIGTRAP, true, "SIGTRAP (breakpoint)"},
    {OE_EXCEPTION_BOUND_OUT_OF_RANGE, X86_TRAP_BR, SIGSEGV, true, "SIGSEGV (bound out of range)"},
    {OE_EXCEPTION_ILLEGAL_INSTRUCTION, X86_TRAP_UD, SIGILL, true, "SIGILL (illegal instruction)"},
    {OE_EXCEPTION_ACCESS_VIOLATION, X86_TRAP_BR, SIGSEGV, true, "SIGSEGV (access violation)"},
    {OE_EXCEPTION_PAGE_FAULT, X86_TRAP_PF, SIGSEGV, true, "SIGSEGV (page fault)"},
    {OE_EXCEPTION_X87_FLOAT_POINT, X86_TRAP_MF, SIGFPE, true, "SIGFPE (x87 floating point)"},
    {OE_EXCEPTION_MISALIGNMENT, X86_TRAP_AC, SIGBUS, true, "SIGBUS (misalignment)"},
    {OE_EXCEPTION_SIMD_FLOAT_POINT, X86_TRAP_XF, SIGFPE, true, "SIGFPE (SIMD float point)"},
};
// clang-format on

static void _sgxlkl_illegal_instr_hook(uint16_t opcode, oe_context_t* context);

static int get_trap_details(
    uint32_t oe_code,
    struct oe_hw_exception_map* trap_map_info)
{
    int index = 0;
    int trap_map_size = ARRAY_SIZE(exception_map);

    for (index = 0; index < trap_map_size; index++)
    {
        if ((exception_map[index].oe_code == oe_code) &&
            (exception_map[index].supported == true))
        {
            *trap_map_info = exception_map[index];
            return 0;
        }
    }
    return -1;
}

static void serialize_ucontext(const oe_context_t* octx, struct ucontext* uctx)
{
    uctx->uc_mcontext.rax = octx->rax;
    uctx->uc_mcontext.rbx = octx->rbx;
    uctx->uc_mcontext.rcx = octx->rcx;
    uctx->uc_mcontext.rdx = octx->rdx;
    uctx->uc_mcontext.rbp = octx->rbp;
    uctx->uc_mcontext.rsp = octx->rsp;
    uctx->uc_mcontext.rdi = octx->rdi;
    uctx->uc_mcontext.rsi = octx->rsi;
    uctx->uc_mcontext.r8 = octx->r8;
    uctx->uc_mcontext.r9 = octx->r9;
    uctx->uc_mcontext.r10 = octx->r10;
    uctx->uc_mcontext.r11 = octx->r11;
    uctx->uc_mcontext.r12 = octx->r12;
    uctx->uc_mcontext.r13 = octx->r13;
    uctx->uc_mcontext.r14 = octx->r14;
    uctx->uc_mcontext.r15 = octx->r15;
    uctx->uc_mcontext.rip = octx->rip;
}

static void deserialize_ucontext(
    const struct ucontext* uctx,
    oe_context_t* octx)
{
    octx->rax = uctx->uc_mcontext.rax;
    octx->rbx = uctx->uc_mcontext.rbx;
    octx->rcx = uctx->uc_mcontext.rcx;
    octx->rdx = uctx->uc_mcontext.rdx;
    octx->rbp = uctx->uc_mcontext.rbp;
    octx->rsp = uctx->uc_mcontext.rsp;
    octx->rdi = uctx->uc_mcontext.rdi;
    octx->rsi = uctx->uc_mcontext.rsi;
    octx->r8 = uctx->uc_mcontext.r8;
    octx->r9 = uctx->uc_mcontext.r9;
    octx->r10 = uctx->uc_mcontext.r10;
    octx->r11 = uctx->uc_mcontext.r11;
    octx->r12 = uctx->uc_mcontext.r12;
    octx->r13 = uctx->uc_mcontext.r13;
    octx->r14 = uctx->uc_mcontext.r14;
    octx->r15 = uctx->uc_mcontext.r15;
    octx->rip = uctx->uc_mcontext.rip;
}

static uint64_t sgxlkl_enclave_signal_handler(
    oe_exception_record_t* exception_record)
{
    int ret = -1;
    siginfo_t info;
    struct ucontext uctx;
    struct oe_hw_exception_map trap_info;
    oe_context_t* oe_ctx = exception_record->context;
    uint16_t* instr_addr = ((uint16_t*)exception_record->context->rip);
    uint16_t opcode = instr_addr ? *instr_addr : 0;

    /**
    * @haohua 
    * OE will catch seg fault before LKL knowing it. We should turn off 
    * sgx-step apic timer at this point to make the result more precise. 
    */
    if (exception_record->code == OE_EXCEPTION_PAGE_FAULT)
    {
        sgxlkl_host_app_main_end(); 
    }
    // haohua

    /* Emulate illegal instructions in SGX hardware mode */
    if (exception_record->code == OE_EXCEPTION_ILLEGAL_INSTRUCTION)
    {
        SGXLKL_TRACE_SIGNAL(
            "Exception SIGILL (illegal instruction) received (code=%d "
            "address=0x%lx opcode=0x%x)\n",
            exception_record->code,
            exception_record->address,
            opcode);

        _sgxlkl_illegal_instr_hook(opcode, exception_record->context);
        return OE_EXCEPTION_CONTINUE_EXECUTION;
    }

    memset(&trap_info, 0, sizeof(trap_info));
    ret = get_trap_details(exception_record->code, &trap_info);
    if (ret != -1)
    {
        SGXLKL_TRACE_SIGNAL(
            "Exception %s received (code=%d address=0x%lx opcode=0x%x)\n",
            trap_info.description,
            exception_record->code,
            exception_record->address,
            opcode);

#ifdef DEBUG
        if (sgxlkl_trace_signal)
        {
            sgxlkl_print_backtrace((void*)oe_ctx->rbp);
        }
#endif

        /**
         * If LKL has not yet been initialised or is terminating (and thus no
         * longer accepts signals), we cannot handle the exception and fail
         * instead.
         */
        if (!lkl_is_running() || is_lkl_terminating())
        {
#ifdef DEBUG
            sgxlkl_error("Exception received but LKL is unable to handle it. "
                         "Printing stack trace saved by exception handler:\n");
            /**
             * Since we cannot unwind the frames in the OE exception handler,
             * we need to print a backtrace with the frame pointer from the
             * saved exception context.
             */
            sgxlkl_print_backtrace((void*)oe_ctx->rbp);
#endif

            struct lthread* lt = lthread_self();
            sgxlkl_fail(
                "Exception %s received before LKL initialisation/after LKL "
                "shutdown (lt->tid=%i [%s] "
                "code=%i "
                "addr=0x%lx opcode=0x%x "
                "ret=%i)\n",
                trap_info.description,
                lt ? lt->tid : -1,
                lt ? lt->attr.funcname : "(?)",
                exception_record->code,
                (void*)exception_record->address,
                opcode,
                ret);
        }

        memset(&uctx, 0, sizeof(uctx));
        serialize_ucontext(oe_ctx, &uctx);

        info.si_errno = 0;
        info.si_code = exception_record->code;
        info.si_addr = (void*)exception_record->address;
        info.si_signo = trap_info.signo;

        /**
         * The trap is is passed to LKL. If it can be handled, excecution will
         * continue, otherwise LKL will abort the process.
         */
        lkl_do_trap(trap_info.trapnr, trap_info.signo, NULL, &uctx, 0, &info);
        deserialize_ucontext(&uctx, oe_ctx);
    }
    else
    {
        struct lthread* lt = lthread_self();
        sgxlkl_fail(
            "Unknown exception %s received (lt->tid=%i [%s]"
            "code=%i "
            "addr=0x%lx opcode=0x%x "
            "ret=%i)\n",
            trap_info.description,
            lt ? lt->tid : -1,
            lt ? lt->attr.funcname : "(?)",
            exception_record->code,
            (void*)exception_record->address,
            opcode,
            ret);
    }

    return OE_EXCEPTION_CONTINUE_EXECUTION;
}

static void _sgxlkl_illegal_instr_hook(uint16_t opcode, oe_context_t* context)
{
    uint32_t rax, rbx, rcx, rdx;
    char* instruction_name = "";

    switch (opcode)
    {   
        /* allow attack from anywhere in the in-enclave application by settng up the APIC timer */
        case UD2_OPCODE:
            sgxlkl_info("[[ SGX-STEP ]] Encounter ud2 instruction \n");
            // using hash can trigger more types of execeptions with issuing only ud2 insrtuction. 
            int hash = (context->r11 * sgx_step_attack_signal_hash_int) % sgx_step_attack_signal_hash_mod; 
            if (hash == sgx_step_attack_signal_hash_target){
                /* leave the enclave by OCALL and send the first APIC signal in host handler */
                sgxlkl_host_sgx_step_attack_setup();
            }else{
                sgxlkl_fail("Encountered an illegal instruction inside enclave (opcode=0x%x [%s])\n", opcode, "ud2");
            }
            break; 
        case OE_CPUID_OPCODE:
            rax = 0xaa, rbx = 0xbb, rcx = 0xcc, rdx = 0xdd;
            if (context->rax != 0xff)
            {
                /* Call into host to execute the CPUID instruction. */
                sgxlkl_host_hw_cpuid(
                    (uint32_t)context->rax, /* leaf */
                    (uint32_t)context->rcx, /* subleaf */
                    &rax,
                    &rbx,
                    &rcx,
                    &rdx);
            }
            context->rax = rax;
            context->rbx = rbx;
            context->rcx = rcx;
            context->rdx = rdx;
            break;
        case RDTSC_OPCODE:
            rax = 0, rdx = 0;
            /* Call into host to execute the RDTSC instruction */
            sgxlkl_host_hw_rdtsc(&rax, &rdx);
            context->rax = rax;
            context->rdx = rdx;
            break;
        default:
            switch (opcode)
            {
                case (0x50f):
                    instruction_name = "syscall";
                    break;
            }

            sgxlkl_fail(
                "Encountered an illegal instruction inside enclave "
                "(opcode=0x%x [%s])\n",
                opcode,
                instruction_name);
    }

    /* Skip over the illegal instruction (note: it will skip the invalid ud2 instruction which is issued by the attacker). */
    context->rip += 2;
}

void _register_enclave_signal_handlers(int mode)
{
    oe_result_t result;

    SGXLKL_VERBOSE("Registering OE exception handler...\n");

    if (sgxlkl_in_sw_debug_mode())
    {
        sgxlkl_host_sw_register_signal_handler(
            (void*)sgxlkl_enclave_signal_handler);
    }
    else
    {
        result = oe_add_vectored_exception_handler(
            true, sgxlkl_enclave_signal_handler);
        if (result != OE_OK)
            sgxlkl_fail("OE exception handler registration failed.\n");
    }
}
