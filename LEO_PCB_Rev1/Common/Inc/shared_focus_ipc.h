/**
 * @file shared_focus_ipc.h
 * @brief Shared memory IPC between M7 and M4 for autofocus
 *
 * MUST BE IDENTICAL ON BOTH CORES
 *
 * Usage: Include this header, then access the mailbox via:
 *   #define g_mb (*(volatile SharedMailbox_t*)SHARED_MAILBOX_ADDR)
 */

#ifndef SHARED_FOCUS_IPC_H
#define SHARED_FOCUS_IPC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */
#define SHARED_MAILBOX_ADDR     0x30010000

#define XY_SAMPLES              64      /* 64 X/Y pairs per FPGA frame        */
#define MAX_MULTI_CAPTURES      16      /* Max frames in a multi-capture req  */

/* Request commands (M4 -> M7) */
#define MB_IDLE                 0
#define MB_REQ_CAPTURE          1       /* Single capture: 1 FPGA frame       */
#define MB_REQ_MULTI_CAPTURE    2       /* Multi capture: req_count frames    */

/* Response status codes (M7 -> M4) */
#define MB_RSP_OK               0       /* Success                            */
#define MB_RSP_NO_DATA          1       /* No FPGA data available             */
#define MB_RSP_UNKNOWN_CMD      2       /* Unknown command                    */
#define MB_RSP_PARTIAL          3       /* Partial: got fewer than requested  */

/* ============================================================================
 * PER-FRAME DATA
 *
 * Each FPGA capture produces 64 X/Y pairs plus their sums.
 * ============================================================================ */
typedef struct {
    uint32_t x[XY_SAMPLES];            /* 64 X values from this frame         */
    uint32_t y[XY_SAMPLES];            /* 64 Y values from this frame         */
    uint64_t sumX;                     /* Sum of all X values                 */
    uint64_t sumY;                     /* Sum of all Y values                 */
    uint32_t timestamp;                /* HAL_GetTick() when frame captured   */
    uint32_t _pad;                     /* Keep 8-byte alignment               */
} FrameData_t;                         /* 536 bytes per frame                 */

/* ============================================================================
 * SHARED MAILBOX STRUCTURE
 *
 * Placed at SHARED_MAILBOX_ADDR in D2 SRAM, accessible by both cores.
 *
 * Single capture (req_cmd = 1):
 *   M4 sets:  req_cmd=1, req_step, req_seq
 *   M7 fills: frames[0], rsp_count=1, rsp_ready=1
 *
 * Multi capture (req_cmd = 2):
 *   M4 sets:  req_cmd=2, req_step, req_seq, req_count (1..16)
 *   M7 fills: frames[0..N-1], rsp_count=N, rsp_ready=1
 *
 * Total size: ~8.5 KB (with 16 frames)
 * ============================================================================ */
typedef struct __attribute__((aligned(32))) {

    /* ---- Request (M4 -> M7) ---- */
    volatile uint32_t req_cmd;          /* MB_IDLE / MB_REQ_CAPTURE / MB_REQ_MULTI_CAPTURE */
    volatile uint32_t req_step;         /* Current focus step index            */
    volatile uint32_t req_seq;          /* Sequence number (monotonic)         */
    volatile uint32_t req_count;        /* Number of captures (multi mode)     */
    uint32_t _pad_req[4];              /* Pad request block to 32 bytes       */

    /* ---- Response (M7 -> M4) ---- */
    volatile uint32_t rsp_ready;        /* 1 = response data ready             */
    volatile uint32_t rsp_seq;          /* Echo of req_seq                     */
    volatile uint32_t rsp_step;         /* Echo of req_step                    */
    volatile uint32_t rsp_status;       /* MB_RSP_OK / MB_RSP_NO_DATA / etc    */
    volatile uint32_t rsp_count;        /* Actual captures completed           */
    uint32_t _pad_rsp[3];              /* Pad response block to 32 bytes      */

    /* ---- Frame data (M7 -> M4) ---- */
    volatile FrameData_t frames[MAX_MULTI_CAPTURES];

} SharedMailbox_t;

/* ============================================================================
 * MAILBOX ACCESS MACRO
 *
 * Use g_mb to access the shared mailbox from either core.
 * Example: g_mb.req_cmd = MB_REQ_CAPTURE;
 * ============================================================================ */
#define g_mb (*(volatile SharedMailbox_t*)SHARED_MAILBOX_ADDR)

#ifdef __cplusplus
}
#endif

#endif /* SHARED_FOCUS_IPC_H */
