#include "scheduler.h"
#include "plsr_service.h"

typedef struct {
    void (*task_fun)(void);
    uint32_t rate_ms;
    uint32_t last_run;
} task_t;

static task_t scheduler_task[]={
    {usart1_task, 2, 0},
    {motor_task_1ms, 1, 0},
    {vofa_task, 50, 0},
    {plsr_service_storage_task_100ms, 100, 0},
};

static uint8_t task_num=0;

void scheduler_init(void) {
    task_num=sizeof(scheduler_task)/sizeof(task_t);
}

void scheduler_run(void) {
    for(uint8_t i=0;i<task_num;i++) {
        uint32_t now_time = HAL_GetTick();
        if(now_time>=scheduler_task[i].rate_ms+scheduler_task[i].last_run) {
            scheduler_task[i].last_run=now_time;
            scheduler_task[i].task_fun();
        }
    }
}
