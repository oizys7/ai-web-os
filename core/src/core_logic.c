#include "core_api.h"

uint64_t aiwos_core_next_tick(uint64_t previous_tick) {
    return previous_tick + 1u;
}

void aiwos_core_apply_event(aiwos_state_snapshot_t *state, const aiwos_event_t *event) {
    if (state == 0 || event == 0) {
        return;
    }

    state->last_event_type = event->type;
    state->last_event_data = event->data;
}

