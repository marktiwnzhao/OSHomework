
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

int strategy;
char table[6] = {'Z','Z','Z','Z','Z','Z'};

PRIVATE void init_tasks()
{
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
	readers = 0;
	writers = 0;


	strategy = 1; //读写公平0、读优先1、写优先2

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
//修改:三种策略的具体执行
PRIVATE read_proc(char proc, int slices){
	table[proc - 'A'] = 'O';
	sleep_ms(slices * TIME_SLICE);
	table[proc - 'A'] = 'Z';
}

PRIVATE	write_proc(char proc, int slices){
	table[proc - 'A'] = 'O';
	sleep_ms(slices * TIME_SLICE);
	table[proc - 'A'] = 'Z';
}

//读写公平方案
void read_gp(char proc, int slices){
	table[proc - 'A'] = 'X';
	P(&queue);
	P(&r_mutex);	
	if (readers==0)
		P(&rw_mutex);
	readers++;
	V(&r_mutex);
	V(&queue);

	P(&n_r_mutex);
	read_proc(proc, slices);
	V(&n_r_mutex);

	P(&r_mutex);
	readers--;
	if (readers==0)
		V(&rw_mutex);
	V(&r_mutex);
    
}
void write_gp(char proc, int slices){
	table[proc - 'A'] = 'X';
	P(&queue);
	P(&rw_mutex);
	V(&queue);
	
	write_proc(proc, slices);
	
	V(&rw_mutex);
}

//读者优先
void read_rf(char proc, int slices){
	table[proc - 'A'] = 'X';
	P(&r_mutex);
	if (readers==0)
        P(&rw_mutex);
    readers++;
	V(&r_mutex);

	P(&n_r_mutex);
    read_proc(proc, slices);
    V(&n_r_mutex);

    P(&r_mutex);
    readers--;
    if (readers==0)
        V(&rw_mutex);
    V(&r_mutex);
}
void write_rf(char proc, int slices){
	table[proc - 'A'] = 'X';
    P(&rw_mutex);
    writers++;
    write_proc(proc, slices);
    writers--;
    V(&rw_mutex);
}

// 写者优先
void read_wf(char proc, int slices){
	table[proc - 'A'] = 'X';
	P(&queue);
	P(&r_mutex);
    if (readers==0)
        P(&rw_mutex);
    readers++;
    V(&r_mutex);
	V(&queue);

    P(&n_r_mutex);
    read_proc(proc, slices);
	V(&n_r_mutex);

    P(&r_mutex);
    readers--;
    if (readers==0)
        V(&rw_mutex);
    V(&r_mutex);  
}
void write_wf(char proc, int slices){
	table[proc - 'A'] = 'X';
    P(&w_mutex);
    if (writers==0)
        P(&queue);
    writers++;
    V(&w_mutex);

    P(&rw_mutex);
    write_proc(proc, slices);
    V(&rw_mutex);

    P(&w_mutex);
    writers--;
    if (writers==0)
        V(&queue);
    V(&w_mutex);
}

read_f read_funcs[3] = {read_gp, read_rf, read_wf};
write_f write_funcs[3] = {write_gp, write_rf, write_wf};

/*======================================================================*
                               ReaderA
 *======================================================================*/
void ReaderA()
{
	while(1){
		read_funcs[strategy]('A', 2);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReaderB
 *======================================================================*/
void ReaderB()
{
	while(1){
		read_funcs[strategy]('B', 3);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReaderC
 *======================================================================*/
void ReaderC()
{
	while(1){
		read_funcs[strategy]('C', 3);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               WriterD
 *======================================================================*/
void WriterD()
{
	while(1){
		write_funcs[strategy]('D', 3);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               WriterE
 *======================================================================*/
void WriterE()
{
	while(1){
		write_funcs[strategy]('E', 4);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReporterF
 *======================================================================*/
void ReporterF()
{
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
						if(table[table_ptr] == 'Z'){
							printf("%c%c ", '\03', table[table_ptr]);
						} else if(table[table_ptr] == 'O'){
							printf("%c%c ", '\02', table[table_ptr]);
						} else if(table[table_ptr] == 'X'){
							printf("%c%c ", '\01', table[table_ptr]);
						}
					}
					printf("\n");
				}

        sleep_ms(TIME_SLICE);
    }
}