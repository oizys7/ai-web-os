#ifndef AIWOS_CORE_API_H
#define AIWOS_CORE_API_H

#include "..\..\abi\aiwos_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t aiwos_core_next_tick(uint64_t previous_tick);
void aiwos_core_apply_event(aiwos_state_snapshot_t *state, const aiwos_event_t *event);

/* 宿主内存抽象 — 委托给 aiwos_init 注册的宿主分配器 */
void *aiwos_alloc(size_t size);
void aiwos_free(void *ptr);
void *aiwos_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif

