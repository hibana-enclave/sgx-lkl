#include <stdio.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include "userargs.h"

sgxlkl_userargs_t* __sgxlkl_userargs;

void sgxlkl_warn(const char* fmt, ...);

int snprintf(char *str, size_t size, const char *format, ...);

/*
**==============================================================================
**
** syscall:
**
**==============================================================================
*/

long lkl_syscall(long no, long* params)
{
    long ret = __sgxlkl_userargs->ua_lkl_syscall(no, params);

    return ret;
}

long __sgxlkl_log_syscall(
    // sgxlkl_syscall_kind type,
    uint32_t type,
    long n,
    long res,
    int params_len,
    ...)
{
    sgxlkl_warn("__sgxlkl_log_syscall() unimplemented in user space");
    return 0;
}

/*
**==============================================================================
**
** bypasses:
**
**==============================================================================
*/

void sgxlkl_app_main_start_notify(void)
{
    return __sgxlkl_userargs->ua_sgxlkl_app_main_start_notify(); 
}

void sgxlkl_app_main_end_notify(void)
{
    return __sgxlkl_userargs->ua_sgxlkl_app_main_end_notify(); 
}


void sgxlkl_warn(const char* msg, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    return __sgxlkl_userargs->ua_sgxlkl_warn(buf);
}

void sgxlkl_error(const char* msg, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    return __sgxlkl_userargs->ua_sgxlkl_error(buf);
}

void sgxlkl_fail(const char* msg, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);

    return __sgxlkl_userargs->ua_sgxlkl_fail(buf);
}

bool sgxlkl_in_sw_debug_mode()
{
    return __sgxlkl_userargs->sw_debug_mode;
}

void* enclave_mmap(
    void* addr,
    size_t length,
    int mmap_fixed,
    int prot,
    int zero_pages)
{
    return __sgxlkl_userargs->ua_enclave_mmap(addr, length, mmap_fixed,
        prot, zero_pages);
}

/*
**==============================================================================
**
** __gdb_get_load_debug_symbols_alive()
**
**==============================================================================
*/

int __gdb_get_load_debug_symbols_alive(void)
{
    return *__sgxlkl_userargs->__gdb_load_debug_symbols_alive_ptr;
}

/*
**==============================================================================
**
** weak form of main (will be overriden by app main)
**
**==============================================================================
*/

__attribute__((weak))
void main()
{
}

__attribute__((weak))
void __sgx_init_enclave()
{
}

/*
**==============================================================================
**
** undefined builtins:
**
**==============================================================================
*/

#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"

void __muldc3()
{
}

void __mulsc3()
{
}

void __mulxc3()
{
}
