
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"

int table[5] = {0, 0, 0, 0, 0};

PRIVATE void init_tasks()
{
	for(int i = 0; i < 5; i++) table[i] = 0;
	sput.value = MAX_CAPACITY;
	sput.head = 0;
	sput.tail = 0;
	sget1.value = 0;
	sget1.head = 0;
	sget1.tail = 0;
	sget2.value = 0;
	sget2.head = 0;
	sget2.tail = 0;
	s1.value = 1;
	s1.head = 0;
	s1.tail = 0;
	s2.value = 1;
	s2.head = 0;
	s2.tail = 0;

	init_screen(tty_table);
	clean(console_table);

	int prior[7] = {1, 1, 1, 1, 1, 1, 1};
	for (int i = 0; i < 7; ++i) {
        proc_table[i].ticks    = prior[i];
        proc_table[i].priority = prior[i];
	}

	// initialization
	k_reenter = 0;
	ticks = 0;

	p_proc_ready = proc_table;
}
/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	disp_str("-----\"kernel_main\" begins-----\n");

	TASK*		p_task		= task_table;
	PROCESS*	p_proc		= proc_table;
	char*		p_task_stack	= task_stack + STACK_SIZE_TOTAL;
	u16		selector_ldt	= SELECTOR_LDT_FIRST;
	int i;
    u8              privilege;
    u8              rpl;
    int             eflags;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
        if (i < NR_TASKS) {     /* 任务 */
                        p_task    = task_table + i;
                        privilege = PRIVILEGE_TASK;
                        rpl       = RPL_TASK;
                        eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
                }
                else {                  /* 用户进程 */
                        p_task    = user_proc_table + (i - NR_TASKS);
                        privilege = PRIVILEGE_USER;
                        rpl       = RPL_USER;
                        eflags    = 0x202; /* IF=1, bit 2 is always 1 */
                }
                
		strcpy(p_proc->p_name, p_task->name);	// name of the process
		p_proc->pid = i;			// pid
		p_proc->sleeping = 0; //修改:初始化新增的结构体属性
		p_proc->blocked = 0;
		
		p_proc->ldt_sel = selector_ldt;

		memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[0].attr1 = DA_C | privilege << 5;
		memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
		p_proc->regs.cs	= (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ds	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.es	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.fs	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ss	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

		p_proc->regs.eip = (u32)p_task->initial_eip;
		p_proc->regs.esp = (u32)p_task_stack;
		p_proc->regs.eflags = eflags;

		p_proc->nr_tty = 0;

		p_task_stack -= p_task->stacksize;
		p_proc++;
		p_task++;
		selector_ldt += 1 << 3;
	}
    
	init_tasks();

	init_clock();
    init_keyboard();

	restart();

	while(1){}
}

/*======================================================================*
                               Reporter
 *======================================================================*/
void Reporter()
{
    // sleep_ms(TIME_SLICE);
    int time = 0;
    while (1) {
				if(time < 20){
					if(time == 19){
						printf("%c%c%c: ",'\06', '2','0');
					}else if(time >= 9 && time < 19){
						printf("%c%c%c: ",'\06', '1','0' + time - 9);
					}else if(time < 9){
						printf("%c%c: ", '\06', '1' + time);
					}
					time++;
					for(int table_ptr = 0; table_ptr < 5; table_ptr++){
						if(table[table_ptr] < 10){
							printf("%c%c ", '\06', table[table_ptr] + '0');
						} else if(table[table_ptr] == 20){
							printf("%c%c%c ", '\06', '2', '0');
						} else {
							printf("%c%c%c ", '\06', '1', '0' + table[table_ptr] - 10);
						}
					}
					printf("\n");
				}

        sleep_ms(TIME_SLICE);
    }
}

void ProducerA() {
	P(&sput);
	P(&s1);
	table[0]++;
	sleep_ms(TIME_SLICE);
	V(&s1);
	V(&sget1);
}
void ProducerB() {
	P(&sput);
	P(&s1);
	table[1]++;
	sleep_ms(TIME_SLICE);
	V(&s1);
	V(&sget2);
}
void CustomerA() {
	P(&sget1);
	P(&s2);
	table[2]++;
	sleep_ms(TIME_SLICE);
	V(&s2);
	V(&sput);
}
void CustomerB(int i) {
	P(&sget2);
	P(&s2);
	table[i]++;
	sleep_ms(TIME_SLICE);
	V(&s2);
	V(&sput);
}
void ProducerP1() {
	while(1) {
		ProducerA();
		//sleep_ms(TIME_SLICE);
	}
}

void ProducerP2() {
	while(1) {
		ProducerB();
		//sleep_ms(TIME_SLICE);
	}
}

void CustomerC1() {
	while(1) {
		CustomerA();
		//sleep_ms(TIME_SLICE);
	}
}

void CustomerC2() {
	while(1) {
		CustomerB(3);
		//sleep_ms(TIME_SLICE);
	}
}

void CustomerC3() {
	while(1) {
		CustomerB(4);
		//sleep_ms(TIME_SLICE);
	}
}