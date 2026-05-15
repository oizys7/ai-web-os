#include "scheduler.h"
#include <string.h>
#include <stdlib.h>

/* ============ 核心调度函数 ============ */

aiwos_scheduler_t* aiwos_scheduler_create(void) {
    aiwos_scheduler_t *sched = (aiwos_scheduler_t *)malloc(sizeof(*sched));
    if (sched == 0) {
        return 0;
    }

    aiwos_scheduler_init(sched);
    return sched;
}

void aiwos_scheduler_init(aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return;
    }

    memset(sched, 0, sizeof(*sched));
    sched->state = AIWOS_SCHED_IDLE;
    sched->current = NULL;
    sched->current_idx = 0;
    sched->min_vruntime = 0;
}

void aiwos_scheduler_shutdown(aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return;
    }

    sched->state = AIWOS_SCHED_SHUTDOWN;
    sched->current = NULL;
    sched->current_idx = 0;
}

void aiwos_scheduler_destroy(aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return;
    }

    /* 清空所有任务 */
    memset(sched->tasks, 0, sizeof(sched->tasks));
    sched->task_count = 0;

    /* 清空统计信息 */
    sched->total_ticks = 0;
    sched->last_tick_ns = 0;
    sched->min_vruntime = 0;

    /* 释放调度器本身 */
    free(sched);
}

aiwos_task_t* aiwos_schedule_next(aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return 0;
    }

    /* TODO: 后续可以改为 rbtree/优先级队列优化 O(n)→O(log n) */
    aiwos_task_t *best = NULL;
    uint64_t min_vruntime = UINT64_MAX;

    for (uint32_t i = 0; i < sched->task_count; i++) {
        aiwos_task_t *task = &sched->tasks[i];
        if (task->state == AIWOS_TASK_READY && task->vruntime < min_vruntime) {
            min_vruntime = task->vruntime;
            best = task;
        }
    }
    return best;
}

void aiwos_scheduler_tick(aiwos_scheduler_t *sched, uint64_t now_ns) {
    if (sched == 0) {
        return;
    }
    if (sched->state == AIWOS_SCHED_SHUTDOWN) {
        return;
    }

    /* 计算时间增量 */
    uint64_t delta_ns = now_ns - sched->last_tick_ns;
    sched->last_tick_ns = now_ns;
    sched->total_ticks++;

    /* 如果有当前运行的任务，更新其 vruntime */
    if (sched->current != 0 && sched->current->state == AIWOS_TASK_RUNNING) {
        /* 根据权重计算 vruntime 增量：delta * (1024 / weight) */
        uint32_t weight = sched->current->weight;
        if (weight == 0) {
            weight = 1024;  /* 默认权重 */
        }
        uint64_t delta_vruntime = delta_ns * 1024 / weight;
        sched->current->vruntime += delta_vruntime;
        sched->current->runtime_accum += delta_ns;

        /* 更新最小 vruntime */
        if (sched->current->vruntime < sched->min_vruntime) {
            sched->min_vruntime = sched->current->vruntime;
        }
    }

    /* 检查所有阻塞任务是否超时 */
    for (uint32_t i = 0; i < sched->task_count; i++) {
        aiwos_task_t *task = &sched->tasks[i];
        if (task->state == AIWOS_TASK_BLOCKED && task->block_timeout > 0) {
            if (now_ns >= task->block_timeout) {
                /* 超时，唤醒任务 */
                task->state = AIWOS_TASK_READY;
                task->block_reason = AIWOS_BLOCK_NONE;
                task->block_timeout = 0;
            }
        }
    }
}

/* ============ 任务管理函数 ============ */

uint32_t aiwos_task_create(aiwos_scheduler_t *sched, void *context) {
    if (sched == 0) {
        return 0;
    }
    if (sched->task_count >= AIWOS_MAX_TASKS) {
        return 0;
    }

    uint32_t id = sched->task_count + 1;
    aiwos_task_t *task = &sched->tasks[sched->task_count];

    task->id = id;
    task->state = AIWOS_TASK_READY;
    task->vruntime = sched->min_vruntime;
    task->runtime_accum = 0;
    task->block_reason = AIWOS_BLOCK_NONE;
    task->block_timeout = 0;
    task->wait_event = 0;
    task->nice = 0;
    task->weight = 1024;
    task->context = context;

    sched->task_count++;
    return id;
}

void aiwos_task_destroy(aiwos_scheduler_t *sched, uint32_t task_id) {
    int32_t found = aiwos_task_find_index(sched, task_id);
    if (found < 0) {
        return;  /* 任务不存在 */
    }

    /* 如果是当前运行的任务，清空 current 指针 */
    if (sched->current == &sched->tasks[found]) {
        sched->current = NULL;
        sched->current_idx = 0;
    }

    /* 用最后一个元素覆盖，再递减计数 */
    uint32_t last = sched->task_count - 1;
    if ((uint32_t)found != last) {
        sched->tasks[found] = sched->tasks[last];
    }
    memset(&sched->tasks[last], 0, sizeof(aiwos_task_t));
    sched->task_count--;
}

aiwos_task_t* aiwos_task_get(aiwos_scheduler_t *sched, uint32_t task_id) {
    if (sched == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < sched->task_count; i++) {
        aiwos_task_t *task = &sched->tasks[i];
        if (task->id == task_id) {
            return task;
        }
    }
    return 0;
}

int32_t aiwos_task_find_index(const aiwos_scheduler_t *sched, uint32_t task_id) {
    if (sched == 0 || task_id == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < sched->task_count; i++) {
        if (sched->tasks[i].id == task_id) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* ============ 任务状态控制函数 ============ */

int32_t aiwos_task_yield(aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return -1;
    }

    aiwos_task_t *curr = sched->current;
    if (curr == 0 || curr->state != AIWOS_TASK_RUNNING) {
        return -1;
    }

    curr->state = AIWOS_TASK_READY;
    sched->current = NULL;
    sched->current_idx = 0;
    return 0;
}

int32_t aiwos_task_block(aiwos_scheduler_t *sched, aiwos_block_reason_t reason,
                         uint32_t wait_event, uint64_t timeout_ns) {
    if (sched == 0) {
        return -1;
    }

    aiwos_task_t *curr = sched->current;
    if (curr == 0 || curr->state != AIWOS_TASK_RUNNING) {
        return -1;
    }

    curr->state = AIWOS_TASK_BLOCKED;
    curr->block_reason = reason;
    curr->wait_event = wait_event;
    curr->block_timeout = timeout_ns;

    sched->current = NULL;
    sched->current_idx = 0;
    return 0;
}

int32_t aiwos_task_wakeup(aiwos_scheduler_t *sched, uint32_t task_id) {
    if (sched == 0) {
        return -1;
    }

    /* TODO: 唤醒指定任务，状态改为 READY */
    aiwos_task_t *wakeup_task = aiwos_task_get(sched, task_id);
    if (wakeup_task == 0) {
        return -1;
    }

    wakeup_task->state = AIWOS_TASK_READY;
    return 0;
}

int32_t aiwos_task_wakeup_by_event(aiwos_scheduler_t *sched, uint32_t event_type) {
    if (sched == 0) {
        return -1;
    }

    /* TODO: 唤醒所有等待该事件的任务 */
    int wakeup_count = 0;
    for (size_t i = 0; i < sched->task_count; i++) {
        if (sched->tasks[i].wait_event == event_type && sched->tasks[i].state == AIWOS_TASK_BLOCKED) {
            sched->tasks[i].state = AIWOS_TASK_READY;
            wakeup_count++;
        }
    }
    return wakeup_count;
}

int32_t aiwos_task_complete(aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return -1;
    }
    
    aiwos_task_t *curr = sched->current;
    if (curr == 0 || curr->state != AIWOS_TASK_RUNNING) {
        return -1;
    }

    curr->state = AIWOS_TASK_DONE;
    sched->current = NULL;
    sched->current_idx = 0;
    return 0;
}

/* ============ 查询函数 ============ */

aiwos_task_t* aiwos_scheduler_current(const aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return 0;
    }
    return sched->current;
}

uint32_t aiwos_scheduler_ready_count(const aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return 0;
    }
    int ready_count = 0; 
    for (uint32_t i = 0; i < sched->task_count; i++) {
        aiwos_task_t *task = &sched->tasks[i];
        if (task->state == AIWOS_TASK_READY) {
            ready_count++;
        }
    }
    return ready_count;
}

uint32_t aiwos_scheduler_blocked_count(const aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return 0;
    }
    
    int blocked_count = 0;
    for (uint32_t i = 0; i < sched->task_count; i++) {
        aiwos_task_t *task = &sched->tasks[i];
        if (task->state == AIWOS_TASK_BLOCKED) {
            blocked_count++;
        }
    }
    return blocked_count;
}

int32_t aiwos_scheduler_has_ready(const aiwos_scheduler_t *sched) {
    if (sched == 0) {
        return 0;
    }
    for (uint32_t i = 0; i < sched->task_count; i++) {
        aiwos_task_t *task = &sched->tasks[i];
        if (task->state == AIWOS_TASK_READY) {
            return 1;
        }
    }
    return 0;
}

/* ============ 权重计算函数 ============ */

uint32_t aiwos_nice_to_weight(int32_t nice) {
    /* Linux 调度器权重表 nice -20~19 → weight 88761~15 */
    static const uint32_t table[40] = {
        /* -20 */ 88761, 71755, 56483, 46273, 36291,
        /* -15 */ 29154, 23254, 18705, 14949, 11916,
        /* -10 */  9548,  7620,  6100,  4904,  3906,
        /*  -5 */  3121,  2501,  1991,  1586,  1277,
        /*   0 */  1024,   820,   655,   526,   423,
        /*   5 */   335,   272,   215,   172,   137,
        /*  10 */   110,    87,    70,    56,    45,
        /*  15 */    36,    29,    23,    18,    15,
    };

    if (nice < -20) { nice = -20; }
    if (nice > 19)  { nice = 19;  }
    return table[nice + 20];
}

uint64_t aiwos_calc_delta_vruntime(uint64_t delta_ns, uint32_t weight) {
    return delta_ns * 1024 / weight;
}
