#include <stdatomic.h>
#include <string.h>

#include <openenclave/bits/eeid.h>
#include <openenclave/corelibc/oemalloc.h>
#include <openenclave/corelibc/oestring.h>
#include <openenclave/internal/globals.h>
#include "openenclave/corelibc/oestring.h"

#include "enclave/enclave_oe.h"
#include "enclave/enclave_signal.h"
#include "enclave/enclave_util.h"
#include "enclave/lthread.h"
#include "enclave/lthread_int.h"
#include "shared/env.h"
#include "shared/timer_dev.h"

#define AUXV_ENTRIES 13

char* at_platform = "x86_64";
sgxlkl_enclave_state_t sgxlkl_enclave_state = {0};

void sgxlkl_app_main_end_notify()
{
    sgxlkl_host_app_main_end(); 
}

void sgxlkl_app_main_start_notify()
{
    sgxlkl_host_app_main_start(); 
}



bool sgxlkl_in_sw_debug_mode()
{
    return sgxlkl_enclave_state.config->mode == SW_DEBUG_MODE;
}

bool sgxlkl_in_hw_debug_mode()
{
    return sgxlkl_enclave_state.config->mode == HW_DEBUG_MODE;
}

bool sgxlkl_in_hw_release_mode()
{
    return sgxlkl_enclave_state.config->mode == HW_RELEASE_MODE;
}

static int _strncmp(const char* x, const char* y, size_t n)
{
    if (n == 0)
        return 0;

    const char* px = x;
    const char* py = y;
    n--;

    while (*px != 0 && *py != 0 && n && *px == *py)
    {
        px++;
        py++;
        n--;
    }

    return *px == *py ? 0 : *px < *py ? -1 : +1;
}

static void init_auxv(size_t* auxv, char* buf_ptr, char* pn)
{
    // By default auxv[AT_RANDOM] points to a buffer with 16 random bytes.
    uint64_t* rbuf = (uint64_t*)buf_ptr;
    buf_ptr += 16;
    // TODO Use intrinsics
    // if (!_rdrand64_step(&rbuf[0]))
    //    goto err;
    register uint64_t rd;
    __asm__ volatile("rdrand %0;" : "=r"(rd));
    rbuf[0] = rd;
    __asm__ volatile("rdrand %0;" : "=r"(rd));
    rbuf[1] = rd;

    memset(auxv, 0, 2 * sizeof(size_t) * AUXV_ENTRIES);
    auxv[0] = AT_CLKTCK;
    auxv[1] = 100;
    auxv[2] = AT_EXECFN;
    auxv[3] = (size_t)pn;
    auxv[4] = AT_HWCAP;
    auxv[5] = 0;
    auxv[6] = AT_EGID;
    auxv[7] = 0;
    auxv[8] = AT_EUID;
    auxv[9] = 0;
    auxv[10] = AT_GID;
    auxv[11] = 0;
    auxv[12] = AT_PAGESZ;
    auxv[13] = 0;
    auxv[14] = AT_PLATFORM;
    memcpy(buf_ptr, at_platform, oe_strlen(at_platform) + 1);
    auxv[15] = (size_t)buf_ptr;
    buf_ptr += oe_strlen(at_platform) + 1;
    auxv[16] = AT_SECURE;
    auxv[17] = 0;
    auxv[18] = AT_UID;
    auxv[19] = 0;
    auxv[20] = AT_RANDOM;
    auxv[21] = (size_t)rbuf;
    auxv[22] = AT_HW_MODE;
    auxv[23] = !sgxlkl_in_sw_debug_mode();
    auxv[24] = AT_NULL;
    auxv[25] = 0;
}

static void _prepare_elf_stack()
{
    sgxlkl_enclave_state_t* state = &sgxlkl_enclave_state;
    const sgxlkl_enclave_config_t* cfg = state->config;

    size_t num_imported_env = 0;
    const char** imported_env = NULL;

    if (sgxlkl_enclave_state.shared_memory.env && cfg->num_host_import_env > 0)
    {
        imported_env = oe_calloc_or_die(
            cfg->num_host_import_env,
            sizeof(char*),
            "Could not allocate memory for imported host environment\n");

        for (size_t i = 0; i < cfg->num_host_import_env; i++)
        {
            const char* name = cfg->host_import_env[i];
            for (char* const* p = sgxlkl_enclave_state.shared_memory.env;
                 p && *p != NULL;
                 p++)
            {
                size_t n = oe_strlen(name);
                if (_strncmp(name, *p, n) == 0 && (*p)[n] == '=')
                {
                    const char* str = *p;
                    imported_env[num_imported_env++] = str;
                }
            }
        }
    }

    size_t num_bytes = 0;
    size_t num_ptrs = 0;
    for (size_t i = 0; i < cfg->num_args; i++)
        num_bytes += oe_strlen(cfg->args[i]) + 1;
    num_ptrs += cfg->num_args + 1;
    for (size_t i = 0; i < cfg->num_env; i++)
        num_bytes += oe_strlen(cfg->env[i]) + 1;
    num_ptrs += cfg->num_env + 1;
    for (size_t i = 0; i < num_imported_env; i++)
        num_bytes += oe_strlen(imported_env[i]) + 1;
    num_ptrs += num_imported_env + 1;
    num_ptrs += 2 * AUXV_ENTRIES;            // auxv vector entries
    num_bytes += oe_strlen(at_platform) + 1; // AT_PLATFORM
    num_bytes += 16;                         // AT_RANDOM

    elf64_stack_t* stack = &sgxlkl_enclave_state.elf64_stack;
    stack->data = oe_calloc_or_die(
        num_bytes,
        sizeof(char),
        "Could not allocate memory for ELF stack strings\n");
    char** out = oe_calloc_or_die(
        num_ptrs,
        sizeof(char*),
        "Could not allocate memory for ELF stack string array\n");

    size_t j = 0;
    char* buf_ptr = stack->data;

#define ADD_STRING(S)                  \
    {                                  \
        size_t len = oe_strlen(S) + 1; \
        memcpy(buf_ptr, (S), len);     \
        out[j++] = buf_ptr;            \
        buf_ptr += len;                \
    }

    // argv
    stack->argv = out;
    for (size_t i = 0; i < cfg->num_args; i++)
        ADD_STRING(cfg->args[i]);
    stack->argc = j;
    out[j++] = NULL;

    // envp
    stack->envp = out + j;
    for (size_t i = 0; i < cfg->num_env; i++)
        ADD_STRING(cfg->env[i]);
    for (size_t i = 0; i < num_imported_env; i++)
        ADD_STRING(imported_env[i]);
    out[j++] = NULL;

    // auxv
    stack->auxv = (Elf64_auxv_t*)(out + j);
    init_auxv((size_t*)(out + j), buf_ptr, stack->argv[0]);
    j += AUXV_ENTRIES * 2;

    // Check that the allocated memory was correct.
    SGXLKL_ASSERT(j + 1 == num_ptrs);
    SGXLKL_ASSERT(out[j] == NULL);
    SGXLKL_ASSERT(out[j - 4] == (char*)AT_HW_MODE);

    oe_free(imported_env);
}

static void _free_elf_stack()
{
    elf64_stack_t* stack = &sgxlkl_enclave_state.elf64_stack;
    // stack->argv[0] is apparently freed during __libc_init_enclave()
    oe_free(stack->data);
}

// We need to have a separate function here
int __sgx_init_enclave()
{
    const sgxlkl_enclave_config_t* config = sgxlkl_enclave_state.config;
    _register_enclave_signal_handlers(config->mode);

    _prepare_elf_stack();

    int r = __libc_init_enclave(
        sgxlkl_enclave_state.elf64_stack.argc,
        sgxlkl_enclave_state.elf64_stack.argv);

    _free_elf_stack();

    return r;
}

#ifdef DEBUG
static void _size_uint64_to_str(uint64_t size, char* buf, uint64_t len)
{
    int i = 0;
    double bytes = size;
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    const int unit_len = sizeof(units) / sizeof(units[0]);

    while (bytes > 1024.0 && i < unit_len)
    {
        bytes /= 1024.0;
        i++;
    }

    /*
     * The following works around oe_snprintf's lack of support for the
     * "%f" format specifier.
     */
    unsigned int whole_part = (unsigned int)bytes;
    double fraction_double = bytes - whole_part;
    unsigned int fraction_part = (unsigned int)(fraction_double * 1000);

    oe_snprintf(buf, len, "%i.%i %s", whole_part, fraction_part, units[i]);
}

static void _sgxlkl_enclave_show_attribute(const void* sgxlkl_enclave_base)
{
    char enclave_size_str[16];

    size_t sgxlkl_enclave_size = __oe_get_enclave_size();
    size_t sgxlkl_enclave_heap_size = __oe_get_heap_size();
    const void* sgxlkl_enclave_heap_base = __oe_get_heap_base();
    const void* sgxlkl_enclave_heap_end = __oe_get_heap_end();

    _size_uint64_to_str(
        sgxlkl_enclave_size, enclave_size_str, sizeof(enclave_size_str));

    SGXLKL_VERBOSE(
        "enclave base=0x%p size=%s\n", sgxlkl_enclave_base, enclave_size_str);

    _size_uint64_to_str(
        sgxlkl_enclave_heap_size, enclave_size_str, sizeof(enclave_size_str));

    SGXLKL_VERBOSE(
        "enclave heap base=0x%p size=%s end=0x%p\n",
        sgxlkl_enclave_heap_base,
        enclave_size_str,
        sgxlkl_enclave_heap_end);
}
#endif

int sgxlkl_ethread_init(void)
{
    void* tls_page;
    __asm__ __volatile__("mov %%gs:0,%0" : "=r"(tls_page));

    struct lthread_tcb_base* sched_tcb = (struct lthread_tcb_base*)tls_page;
    sched_tcb->self = (void*)tls_page;

    size_t tls_offset = SCHEDCTX_OFFSET;
    sched_tcb->schedctx = (struct schedctx*)((char*)tls_page + tls_offset);

    /* Wait until libc has been initialized */
    while (sgxlkl_enclave_state.libc_state != libc_initialized)
    {
        a_spin();
    }

    /* Initialization completed, now run the scheduler */
    init_ethread_tp();
    _lthread_sched_init(sgxlkl_enclave_state.config->stacksize);

    return lthread_run();
}

static void _read_eeid_config()
{
    const oe_eeid_t* eeid = (oe_eeid_t*)__oe_get_eeid();
    const char* config_json = (const char*)eeid->data;

    sgxlkl_enclave_config_t* cfg = oe_malloc(sizeof(sgxlkl_enclave_config_t));
    if (!cfg)
        sgxlkl_fail("out of memory, cannot allocate enclave config.\n");
    sgxlkl_read_enclave_config(config_json, cfg, true);
    sgxlkl_enclave_state.config = cfg;
}

static void _copy_shared_memory(const sgxlkl_shared_memory_t* host)
{
    const sgxlkl_enclave_config_t* cfg = sgxlkl_enclave_state.config;

    /* Deep copy where necessary */

    sgxlkl_shared_memory_t* enc = &sgxlkl_enclave_state.shared_memory;
    memset(enc, 0, sizeof(sgxlkl_shared_memory_t));

    if (cfg->io.network)
        enc->virtio_net_dev_mem = host->virtio_net_dev_mem;

    if (cfg->io.console)
        enc->virtio_console_mem = host->virtio_console_mem;

    enc->evt_channel_num = host->evt_channel_num;
    /* enc_dev_config is required to be outside the enclave */
    enc->enc_dev_config = host->enc_dev_config;

    enc->virtio_swiotlb = host->virtio_swiotlb;
    enc->virtio_swiotlb_size = host->virtio_swiotlb_size;

    /* timer_dev_mem is required to be outside the enclave */
    enc->timer_dev_mem = host->timer_dev_mem;

    if (cfg->io.block)
    {
        enc->num_virtio_blk_dev = host->num_virtio_blk_dev;

        enc->virtio_blk_dev_mem = oe_calloc_or_die(
            enc->num_virtio_blk_dev,
            sizeof(void*),
            "Could not allocate memory for virtio block devices\n");
        enc->virtio_blk_dev_names = oe_calloc_or_die(
            enc->num_virtio_blk_dev,
            sizeof(char*),
            "Could not allocate memory for virtio block devices\n");
        for (size_t i = 0; i < enc->num_virtio_blk_dev; i++)
        {
            enc->virtio_blk_dev_mem[i] = host->virtio_blk_dev_mem[i];
            const char* name = host->virtio_blk_dev_names[i];
            size_t name_len = oe_strlen(name) + 1;
            enc->virtio_blk_dev_names[i] = oe_calloc_or_die(
                name_len,
                sizeof(char),
                "Could not allocate memory for virtio block device name\n");
            memcpy(enc->virtio_blk_dev_names[i], name, name_len);
        }
    }

    if (host->env)
    {
        size_t henvc = 0;
        while (host->env[henvc++] != 0)
            ;
        char** tmp = oe_calloc_or_die(
            henvc + 1,
            sizeof(char*),
            "Could not allocate memory for host import environment variable\n");
        for (size_t i = 0; i < henvc; i++)
            tmp[i] = oe_strdup(host->env[i]);
        tmp[henvc] = NULL;
        enc->env = tmp;
    }
}

static void _free_shared_memory()
{
    sgxlkl_shared_memory_t* shm = &sgxlkl_enclave_state.shared_memory;

    for (size_t i = 0; i < shm->num_virtio_blk_dev; i++)
        oe_free(shm->virtio_blk_dev_names[i]);

    oe_free(shm->virtio_blk_dev_mem);
    oe_free(shm->virtio_blk_dev_names);

    for (size_t i = 0; shm->env[i] != 0; i++)
        oe_free(shm->env[i]);
    oe_free((char**)shm->env);
}

int sgxlkl_enclave_init(const sgxlkl_shared_memory_t* shared_memory)
{
    SGXLKL_ASSERT(shared_memory);

    memset(&sgxlkl_enclave_state, 0, sizeof(sgxlkl_enclave_state));
    sgxlkl_enclave_state.libc_state = libc_not_started;

#ifdef DEBUG
    /* Make sure verbosity is off before loading the config (we don't know
     * whether it's enabled yet).*/
    sgxlkl_enclave_state.verbose = false;
#endif

    _read_eeid_config();
    _copy_shared_memory(shared_memory);

#ifdef DEBUG
    // Initialise verbosity setting, so SGXLKL_VERBOSE can be used from this
    // point onwards
    sgxlkl_enclave_state.verbose = sgxlkl_enclave_state.config->verbose;
#endif

    SGXLKL_VERBOSE("enter\n");

    void* tls_page;
    __asm__ __volatile__("mov %%gs:0,%0" : "=r"(tls_page));

    struct lthread_tcb_base* sched_tcb = (struct lthread_tcb_base*)tls_page;
    sched_tcb->self = (void*)tls_page;

    size_t tls_offset = SCHEDCTX_OFFSET;
    sched_tcb->schedctx = (struct schedctx*)((char*)tls_page + tls_offset);

    const void* sgxlkl_enclave_base = __oe_get_enclave_base();

#ifdef DEBUG
    _sgxlkl_enclave_show_attribute(sgxlkl_enclave_base);
#endif

    /* Indicate ongoing libc initialisation */
    sgxlkl_enclave_state.libc_state = libc_initializing;

    SGXLKL_VERBOSE("calling _dlstart_c()\n");
    _dlstart_c((size_t)sgxlkl_enclave_base);

    return __sgx_init_enclave();
}

void sgxlkl_free_enclave_state()
{
    sgxlkl_enclave_state_t* state = &sgxlkl_enclave_state;

    state->elf64_stack.argc = 0;
    oe_free(state->elf64_stack.argv); /* includes envp/auxv */

    state->num_disk_state = 0;
    oe_free(state->disk_state);

    state->libc_state = libc_not_started;

    _free_shared_memory();
}

void sgxlkl_debug_dump_stack_traces(void)
{
#ifdef DEBUG
    SGXLKL_VERBOSE("Dumping all stack traces from threads...\n");
    lthread_dump_all_threads(false);
#endif
}
