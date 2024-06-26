#ifndef ENCLAVE_OE_H
#define ENCLAVE_OE_H

#include "enclave/enclave_state.h"

// OE uses the page pointed to by %fs:0 to store thread-specific information
// for things like handling AEX and saving register state during ocalls.
// The end of this page (over 3000 bytes as of this writing) is only used by
// OE's internal pthread implementation, which SGX-LKL doesn't use. Use the
// end of this page to store the schedctx to avoid interference with OE.
#define SCHEDCTX_OFFSET (4096 - sizeof(struct schedctx))

extern void* _dlstart_c(size_t base);

extern int __libc_init_enclave(int argc, char** argv);

bool sgxlkl_in_sw_debug_mode();
bool sgxlkl_in_hw_debug_mode();
bool sgxlkl_in_hw_release_mode();

void sgxlkl_app_main_end_notify(); 
void sgxlkl_app_main_start_notify(); 

#endif /* ENCLAVE_OE_H */
