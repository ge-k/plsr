#include "vofa_app.h"
#include "plsr_service.h"

// status.current_freq_hz：当前实时频率，单位 Hz。这个值来自当前加减速曲线计算后的频率，也是当前准备/正在输出给 PWM 定时器的频率。
// status.current_segment：当前执行段号。正常运行时是 1~segment_count；空闲、完成后通常是 0。
// status.state：当前 PLSR 状态枚举值：
// 0：IDLE，空闲
// 1：DIR_DELAY，方向延时中
// 2：RUNNING，正在输出脉冲
// 3：WAIT_EXT，等待 EXT
// 4：DONE，完成
// 5：ERROR，错误

// 调度器里是50ms运行一次，意思是VOFA里每个点对应50ms
void vofa_task(void)
{
    PlsrStatus status;

    plsr_service_get_status(&status);
    my_printf("%lu,%u,%u\r\n",
              (unsigned long)status.current_freq_hz,
              (unsigned int)status.current_segment,
              (unsigned int)status.state);
}

