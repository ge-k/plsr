#include "my_main.h"

void my_main(void){
	my_usart1_init();
	motor_task_init();
	scheduler_init();
	my_printf("PLSR ready\r\n");
	while(1){
		scheduler_run();
	}
}
