#ifndef _SGXLKL_UTIL_H
#define _SGXLKL_UTIL_H

#include <stdint.h>
#include <unistd.h>

typedef enum {
    STEP_PHASE_0, // stopped
    STEP_PHASE_1, // wait to issue apic interrupt (delay an interval)
    STEP_PHASE_2,  // sending apic interrupts 
    STEP_PHASE_3  // terminated
} APIC_Triggered_State;


void sgxlkl_host_fail(char* msg, ...) __attribute__((noreturn));
void sgxlkl_host_err(char* msg, ...);
void sgxlkl_host_warn(char* msg, ...);
void sgxlkl_host_info(char* msg, ...);
void sgxlkl_host_verbose(char* msg, ...);
void sgxlkl_host_verbose_raw(char* msg, ...);

#endif
