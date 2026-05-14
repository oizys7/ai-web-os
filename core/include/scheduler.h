#ifndef AIWOS_SCHEDULER_H
#define AIWOS_SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 任务状态枚举 ============ */
typedef enum aiwos_task_state {
    AIWOS_TASK_READY = 0,      /* 就绪，可被调度 */
    AIWOS_TASK_RUNNING = 1,    /* 正在运行 */
    AIWOS_TASK_BLOCKED = 2,    /* 阻塞，等待资源或事件 */
    AIWOS_TASK_DONE = 4        /* 完成，可被回收 */
} aiwos_task_state_t;

/* ============ 调度器状态枚举 ============ */
typedef enum aiwos_scheduler_state {
    AIWOS_SCHED_IDLE = 0,      /* 空闲，无任务 */
    AIWOS_SCHED_RUNNING = 1,   /* 正在调度 */
    AIWOS_SCHED_SHUTDOWN = 2   /* 关闭中 */
} aiwos_scheduler_state_t;

/* ============ 阻塞原因枚举 ============ */
typedef enum aiwos_block_reason {
    AIWOS_BLOCK_NONE = 0,          /* 未阻塞 */
    AIWOS_BLOCK_EVENT = 1,         /* 等待事件 */
    AIWOS_BLOCK_TIMEOUT = 2,       /* 等待超时 */
    AIWOS_BLOCK_RESOURCE = 3,      /* 等待资源 (GPU/内存等) */
    AIWOS_BLOCK_AI_INFERENCE = 4   /* 等待 AI 推理完成 */
} aiwos_block_reason_t;

/* ============ 任务定义 ============ */
typedef struct aiwos_task {
    uint32_t                id;             /* 任务 ID */
    aiwos_task_state_t      state;          /* 当前状态 */
    uint64_t                vruntime;       /* 虚拟运行时间，越小越优先 */
    uint64_t                runtime_accum;  /* 累计实际运行时间 (ns) */

    /* 阻塞相关信息 */
    aiwos_block_reason_t    block_reason;   /* 阻塞原因 */
    uint64_t                block_timeout;  /* 阻塞超时时间戳 (ns) */
    uint32_t                wait_event;     /* 等待的事件类型 */

    /* 优先级权重 (NICE: -20~19，权重 1024~20) */
    int32_t                 nice;           /* nice 值 */
    uint32_t                weight;         /* 权重 */

    void                   *context;        /* 任务上下文指针 */
} aiwos_task_t;

/* ============ 调度器定义 ============ */
#define AIWOS_MAX_TASKS 64

typedef struct aiwos_scheduler {
    aiwos_scheduler_state_t state;          /* 调度器状态 */
    aiwos_task_t           *current;        /* 当前运行的任务 */
    uint32_t                current_idx;    /* 当前任务在数组中的索引 */

    aiwos_task_t            tasks[AIWOS_MAX_TASKS];
    uint32_t                task_count;

    /* 最小 vruntime，用于新任务/唤醒任务的初始值 */
    uint64_t                min_vruntime;

    /* 调度统计 */
    uint64_t                total_ticks;    /* 总 tick 次数 */
    uint64_t                last_tick_ns;   /* 上次 tick 时间戳 */
} aiwos_scheduler_t;

/* ============ 核心调度函数声明 ============ */

/* 调度器初始化 */
void aiwos_scheduler_init(aiwos_scheduler_t *sched);

/* 调度器关闭 */
void aiwos_scheduler_shutdown(aiwos_scheduler_t *sched);

/* 选择下一个要运行的任务 */
aiwos_task_t* aiwos_schedule_next(aiwos_scheduler_t *sched);

/* 执行一次调度 tick，更新 vruntime 并处理状态转换 */
void aiwos_scheduler_tick(aiwos_scheduler_t *sched, uint64_t now_ns);

/* ============ 任务管理函数声明 ============ */

/* 创建新任务，返回任务 ID */
uint32_t aiwos_task_create(aiwos_scheduler_t *sched, void *context);

/* 销毁任务 */
void aiwos_task_destroy(aiwos_scheduler_t *sched, uint32_t task_id);

/* 获取任务指针 */
aiwos_task_t* aiwos_task_get(aiwos_scheduler_t *sched, uint32_t task_id);

/* ============ 任务状态控制函数声明 ============ */

/* 让当前任务让出 CPU */
int32_t aiwos_task_yield(aiwos_scheduler_t *sched);

/* 阻塞当前任务 */
int32_t aiwos_task_block(aiwos_scheduler_t *sched, aiwos_block_reason_t reason,
                         uint32_t wait_event, uint64_t timeout_ns);

/* 唤醒指定任务 */
int32_t aiwos_task_wakeup(aiwos_scheduler_t *sched, uint32_t task_id);

/* 唤醒所有等待特定事件的任务 */
int32_t aiwos_task_wakeup_by_event(aiwos_scheduler_t *sched, uint32_t event_type);

/* 完成当前任务 */
int32_t aiwos_task_complete(aiwos_scheduler_t *sched);

/* ============ 查询函数声明 ============ */

/* 获取当前运行的任务 */
aiwos_task_t* aiwos_scheduler_current(const aiwos_scheduler_t *sched);

/* 获取就绪队列任务数量 */
uint32_t aiwos_scheduler_ready_count(const aiwos_scheduler_t *sched);

/* 获取阻塞队列任务数量 */
uint32_t aiwos_scheduler_blocked_count(const aiwos_scheduler_t *sched);

/* 检查是否有就绪任务 */
int32_t aiwos_scheduler_has_ready(const aiwos_scheduler_t *sched);

/* ============ 权重计算函数声明 ============ */

/* 根据 nice 值计算权重 */
uint32_t aiwos_nice_to_weight(int32_t nice);

/* 计算权重下的 vruntime 增量 */
uint64_t aiwos_calc_delta_vruntime(uint64_t delta_ns, uint32_t weight);

#ifdef __cplusplus
}
#endif

#endif
