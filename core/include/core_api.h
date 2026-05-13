#ifndef AIWOS_CORE_API_H
#define AIWOS_CORE_API_H

#include "..\..\abi\aiwos_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t aiwos_core_next_tick(uint64_t previous_tick);
void aiwos_core_apply_event(aiwos_state_snapshot_t *state, const aiwos_event_t *event);

#ifdef __cplusplus
}
#endif

#endif

