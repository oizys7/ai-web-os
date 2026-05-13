#include "..\..\abi\aiwos_abi.h"
#include "..\..\hal\native\include\hal_native.h"

#include <stdio.h>

int main(void) {
    aiwos_host_api_t host;
    aiwos_event_t event;
    aiwos_state_snapshot_t state;
    int32_t rc;

    aiwos_hal_native_fill_host_api(&host);

    rc = aiwos_init(&host);
    if (rc != AIWOS_OK) {
        printf("init failed: %d\n", (int)rc);
        return 1;
    }

    rc = aiwos_tick(host.now_ns());
    if (rc != AIWOS_OK) {
        printf("tick failed: %d\n", (int)rc);
        aiwos_shutdown();
        return 1;
    }

    event.type = AIWOS_EVENT_TIMER;
    event.data = 42u;
    rc = aiwos_handle_event(&event);
    if (rc != AIWOS_OK) {
        printf("event failed: %d\n", (int)rc);
        aiwos_shutdown();
        return 1;
    }

    rc = aiwos_query_state(0u, &state, sizeof(state));
    if (rc != AIWOS_OK) {
        printf("query failed: %d\n", (int)rc);
        aiwos_shutdown();
        return 1;
    }

    printf("hello ai-web-os\n");
    printf("tick_count=%llu last_now_ns=%llu last_event_type=%u last_event_data=%llu\n",
           (unsigned long long)state.tick_count,
           (unsigned long long)state.last_now_ns,
           (unsigned int)state.last_event_type,
           (unsigned long long)state.last_event_data);

    aiwos_shutdown();
    return 0;
}

