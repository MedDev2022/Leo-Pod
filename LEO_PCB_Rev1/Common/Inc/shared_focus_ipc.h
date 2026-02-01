/**
 * @file shared_focus_ipc.h
 * @brief Simple shared memory IPC between M7 and M4 for autofocus
 *
 * MUST BE IDENTICAL ON BOTH CORES
 */

#ifndef SHARED_FOCUS_IPC_H
#define SHARED_FOCUS_IPC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XY_SAMPLES      64      /* 64 X/Y pairs per capture */
#define FOCUS_STEPS     10      /* 10 steps in autofocus scan */

/* Request commands */
#define MB_IDLE         0
#define MB_REQ_CAPTURE  1

/**
 * Shared mailbox structure
 * - M4 writes: req_cmd, req_step, req_seq
 * - M7 writes: x[], y[], sumX, sumY, rsp_ready, rsp_seq
 */
typedef struct __attribute__((aligned(32))) {
    /* Request (M4 -> M7) */
    volatile uint32_t req_cmd;      /* 0=idle, 1=capture */
    volatile uint32_t req_step;     /* Current focus step 0-9 */
    volatile uint32_t req_seq;      /* Sequence number */
    uint32_t _pad1[5];              /* Pad to 32 bytes */

    /* Response (M7 -> M4) */
    volatile uint32_t rsp_ready;    /* 1 = data ready */
    volatile uint32_t rsp_seq;      /* Echo of req_seq */
    volatile uint32_t rsp_step;     /* Echo of req_step */
    volatile uint32_t rsp_status;   /* 0 = OK, non-zero = error */
    volatile uint64_t  sumX;         /* Sum of all X values */
    volatile uint64_t  sumY;         /* Sum of all Y values */

    /* Data (M7 -> M4) */
    volatile uint32_t x[XY_SAMPLES]; /* 64 X values */
    volatile uint32_t y[XY_SAMPLES]; /* 64 Y values */

} SharedMailbox_t;

/* ============================================================================
 * SHARED MAILBOX INSTANCE
 * Placed in SRAM4 (0x38000000) - accessible by both M7 and M4
 *
 * In ONE source file per core, define SHARED_FOCUS_IPC_OWNER before including:
 *   #define SHARED_FOCUS_IPC_OWNER
 *   #include "shared_focus_ipc.h"
 *
 * All other files just include normally (will get extern)
 * ============================================================================ */

//extern __attribute__((section(".shared_ram"))) volatile SharedMailbox_t g_mb;

//
//#ifdef SHARED_FOCUS_IPC_OWNER
    __attribute__((section(".shared_ram")))
    volatile SharedMailbox_t g_mb;// = {0};
//#else
//   // extern volatile SharedMailbox_t g_mb;
//
//
//#endif

#ifdef __cplusplus
}
#endif

#endif /* SHARED_FOCUS_IPC_H */
