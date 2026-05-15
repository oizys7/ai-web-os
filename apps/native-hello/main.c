/*
 * ai-web-os Native Host Example
 *
 * A reusable reference implementation of an ai-web-os host.
 *
 * ── Architecture ──
 *   Host (this file)     →  platform-independent core driver
 *   HAL (hal_*.c)        →  platform-specific: time, log, memory
 *   ABI (aiwos_abi.h)    →  contract between host and core
 *   Core (aiwos_core)    →  scheduler, state machine, events
 *
 * ── To port to a new platform ──
 *   1. Create a HAL file implementing time, log, and memory
 *   2. Fill aiwos_host_api_t with your HAL functions
 *   3. Keep the demo_host() pattern below — it works unchanged
 *      with any HAL that satisfies the ABI contract
 *
 * ── This example runs the same core operations on two HALs ──
 *   Native HAL   → real system time, stdout log, malloc/free
 *   Simple HAL   → simulated time, printf log, malloc/free
 *   Demonstrates: host is replaceable, core behavior is unchanged
 */

#include "aiwos_abi.h"
#include "hal_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Utility
 * ================================================================ */

static const char *status_str(int32_t rc) {
    switch (rc) {
        case AIWOS_OK:                     return "OK";
        case AIWOS_ERR_INVALID_ARGUMENT:   return "INVALID_ARGUMENT";
        case AIWOS_ERR_INCOMPATIBLE_ABI:   return "INCOMPATIBLE_ABI";
        case AIWOS_ERR_BUFFER_TOO_SMALL:   return "BUFFER_TOO_SMALL";
        case AIWOS_ERR_NOT_INITIALIZED:    return "NOT_INITIALIZED";
        case AIWOS_ERR_NOT_FOUND:          return "NOT_FOUND";
        case AIWOS_ERR_LIMIT_REACHED:      return "LIMIT_REACHED";
        case AIWOS_ERR_NOT_SUPPORTED:      return "NOT_SUPPORTED";
        default:                           return "UNKNOWN";
    }
}

/* ================================================================
 *  Simple Inline HAL — demonstrates host replaceability
 *
 *  This HAL uses only stdlib. It provides deterministic simulated
 *  time so the output is reproducible across runs.
 * ================================================================ */

static uint64_t g_simple_now;

static void simple_log(const char *msg, size_t len) {
    printf("[simple] %.*s\n", (int)len, msg);
}

static uint64_t simple_now_ns(void) {
    return g_simple_now;
}

static void *simple_alloc(size_t size) {
    return malloc(size);
}

static void simple_free(void *ptr) {
    free(ptr);
}

static void *simple_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

static void fill_simple_host(aiwos_host_api_t *host) {
    memset(host, 0, sizeof(*host));
    host->host_abi_version = AIWOS_ABI_VERSION;
    host->log = simple_log;
    host->now_ns = simple_now_ns;
    host->alloc = simple_alloc;
    host->free = simple_free;
    host->realloc = simple_realloc;
}

/* ================================================================
 *  Reusable Host Driver
 *
 *  Runs the standard host lifecycle on any valid aiwos_host_api_t.
 *  The same function is called with two different HALs below.
 * ================================================================ */

static int demo_host(const aiwos_host_api_t *host, const char *name) {
    int32_t rc;
    aiwos_event_t event;
    aiwos_state_snapshot_t snap;
    aiwos_scheduler_stats_t stats;

    printf("\n>>> ======================== %s ========================\n", name);

    /* ── Phase 1: ABI Version Check ── */
    printf("\n-- Phase 1: ABI Version --\n");
    {
        uint32_t ver = aiwos_abi_version();
        printf("  core abi_version           = %u", (unsigned)ver);
        if (ver != AIWOS_ABI_VERSION) {
            printf("  (expected %u — MISMATCH!)\n", (unsigned)AIWOS_ABI_VERSION);
            return 1;
        }
        printf("\n");
    }

    /* ── Phase 2: Core Initialization ── */
    printf("\n-- Phase 2: Core Init --\n");
    {
        rc = aiwos_init(host);
        printf("  aiwos_init                 = %s", status_str(rc));
        if (rc != AIWOS_OK) {
            printf("  <== HALT\n");
            return 1;
        }
        printf("\n");
    }

    /* ── Phase 3: Event Loop ── */
    printf("\n-- Phase 3: Event Loop --\n");
    {
        /* Tick 1 — advance time */
        rc = aiwos_tick(host->now_ns());
        printf("  aiwos_tick                 = %s  (tick=1)\n", status_str(rc));

        /* Timer event */
        event.type = AIWOS_EVENT_TIMER;
        event.data = 42;
        rc = aiwos_handle_event(&event);
        printf("  aiwos_handle_event(TIMER)  = %s  (data=%llu)\n",
               status_str(rc), (unsigned long long)event.data);

        /* Tick 2 */
        rc = aiwos_tick(host->now_ns());
        printf("  aiwos_tick                 = %s  (tick=2)\n", status_str(rc));

        /* Interrupt event */
        event.type = AIWOS_EVENT_INTERRUPT;
        event.data = 0;
        rc = aiwos_handle_event(&event);
        printf("  aiwos_handle_event(IRQ)    = %s\n", status_str(rc));

        /* Tick 3 */
        rc = aiwos_tick(host->now_ns());
        printf("  aiwos_tick                 = %s  (tick=3)\n", status_str(rc));
    }

    /* ── Phase 4: System State ── */
    printf("\n-- Phase 4: System State --\n");
    {
        /* State snapshot */
        memset(&snap, 0, sizeof(snap));
        rc = aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap));
        printf("  aiwos_query_state(SNAPSHOT) = %s\n", status_str(rc));
        if (rc == AIWOS_OK) {
            printf("    tick_count      = %llu\n",   (unsigned long long)snap.tick_count);
            printf("    last_now_ns     = %llu\n",   (unsigned long long)snap.last_now_ns);
            printf("    last_event_type = %u\n",      (unsigned)snap.last_event_type);
            printf("    scheduler_state = %u\n",      (unsigned)snap.scheduler_state);
            printf("    task_count      = %u\n",      (unsigned)snap.task_count);
            printf("    ready_count     = %u\n",      (unsigned)snap.ready_count);
            printf("    blocked_count   = %u\n",      (unsigned)snap.blocked_count);
        }

        /* Scheduler stats */
        memset(&stats, 0, sizeof(stats));
        rc = aiwos_query_state(AIWOS_QUERY_SCHEDULER_STATS, &stats, sizeof(stats));
        printf("  aiwos_query_state(STATS)   = %s\n", status_str(rc));
        if (rc == AIWOS_OK) {
            printf("    total_ticks     = %llu\n",   (unsigned long long)stats.total_ticks);
            printf("    task_count      = %u\n",      (unsigned)stats.task_count);
            printf("    ready_count     = %u\n",      (unsigned)stats.ready_count);
            printf("    blocked_count   = %u\n",      (unsigned)stats.blocked_count);
        }
    }

    /* ── Phase 5: Shutdown ── */
    printf("\n-- Phase 5: Shutdown --\n");
    {
        aiwos_shutdown();
        printf("  aiwos_shutdown()           = OK\n");

        /* Verify post-shutdown calls are rejected */
        rc = aiwos_tick(host->now_ns());
        printf("  aiwos_tick (after stop)    = %s", status_str(rc));
        if (rc == AIWOS_ERR_NOT_INITIALIZED) {
            printf("  (correctly rejected)\n");
        } else {
            printf("  <== expected NOT_INITIALIZED\n");
        }
    }

    printf("\n<<< ==================== %s DONE ====================\n", name);
    return 0;
}

/* ================================================================
 *  Main — run the same driver with two different HALs
 * ================================================================ */

int main(void) {
    aiwos_host_api_t native_host;
    aiwos_host_api_t simple_host;
    int ret;

    printf("=== ai-web-os Host Example (abi v%u) ===\n\n",
           (unsigned)AIWOS_ABI_VERSION);
    printf("This example runs the same core operations on two\n");
    printf("different HAL implementations to demonstrate that the\n");
    printf("host layer is replaceable while core behavior is unchanged.\n");

    /* ── Native HAL ── */
    aiwos_hal_native_fill_host_api(&native_host);
    g_simple_now = 1000000;
    ret = demo_host(&native_host, "Native HAL");

    /* ── Simple Inline HAL ── */
    g_simple_now = 1000000;
    fill_simple_host(&simple_host);
    ret += demo_host(&simple_host, "Simple HAL");

    printf("\n=== Host Example Complete ===\n");
    return ret;
}
