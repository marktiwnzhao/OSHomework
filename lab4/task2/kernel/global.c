
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            global.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define GLOBAL_VARIABLES_HERE

#include "type.h"
#include "const.h"
#include "protect.h"
#include "tty.h"
#include "console.h"
#include "proc.h"
#include "global.h"
#include "proto.h"


PUBLIC	PROCESS	proc_table[NR_TASKS + NR_PROCS];

PUBLIC	TASK	task_table[NR_TASKS] = {
	{task_tty, STACK_SIZE_TTY, "tty"}};

PUBLIC  TASK    user_proc_table[NR_PROCS] = {//修改:在进程表中注册进程
					{ProducerP1, STACK_SIZE_TESTA, "ProducerP1"},
					{ProducerP2, STACK_SIZE_TESTB, "ProducerP2"},
					{CustomerC1, STACK_SIZE_TESTC, "CustomerC1"},
                    {CustomerC2, STACK_SIZE_TESTD, "CustomerC2"},
                    {CustomerC3, STACK_SIZE_TESTE, "CustomerC3"},
                    {Reporter, STACK_SIZE_TESTF, "Reporter"}};

PUBLIC	char		task_stack[STACK_SIZE_TOTAL];

PUBLIC	TTY		tty_table[NR_CONSOLES];
PUBLIC	CONSOLE		console_table[NR_CONSOLES];

PUBLIC	irq_handler	irq_table[NR_IRQ];


//修改:信号量初始化
PUBLIC  SEMAPHORE sput = {MAX_CAPACITY, 0, 0};
PUBLIC  SEMAPHORE sget1 = {0, 0, 0};
PUBLIC  SEMAPHORE sget2 = {0, 0, 0};
PUBLIC  SEMAPHORE s1 = {1, 0, 0};
PUBLIC  SEMAPHORE s2 = {1, 0, 0};

//系统调用表
PUBLIC	system_call	sys_call_table[NR_SYS_CALL] = {
        sys_get_ticks,
        sys_write_str,
        sys_sleep,
        p_process,
        v_process
};