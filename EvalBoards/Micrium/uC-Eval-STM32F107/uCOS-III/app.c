/*
*********************************************************************************************************
*                                              EXAMPLE CODE
*
*                          (c) Copyright 2003-2013; Micrium, Inc.; Weston, FL
*
*               All rights reserved.  Protected by international copyright laws.
*               Knowledge of the source code may NOT be used to develop a similar product.
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                            EXAMPLE CODE
*
*                                     ST Microelectronics STM32
*                                              on the
*
*                                     Micrium uC-Eval-STM32F107
*                                        Evaluation Board
*
* Filename      : app.c
* Version       : V1.00
* Programmer(s) : EHS
*                 DC
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include <includes.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define TASK_FIRST_RR_TIME_QUANTA 1000
#define TASK_SECOND_RR_TIME_QUANTA 2000
#define TASK_THIRD_RR_TIME_QUANTA 500

#define TASK_COUNT_PERIOD  1000000

/*
*********************************************************************************************************
*                                                 TCB
*********************************************************************************************************
*/

static OS_MEM MemPool;
static char memPoolStorage[MemPool_Size][1000];
static OS_SEM memPoolSem;

static OS_TCB AppTaskStartTCB;
static OS_TCB AppTaskFirstTCB;
static OS_TCB SDCardTaskTCB;

/*
*********************************************************************************************************
*                                                STACKS
*********************************************************************************************************
*/

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  AppTaskFirstStk[APP_TASK_FIRST_STK_SIZE];

static CPU_STK SDCardTaskSTK[APP_TASK_FIRST_STK_SIZE];


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskCreate (void);
static  OS_ERR AppObjCreate  (void);
static  void  AppTaskStart  (void *p_arg);

static  void  AppTaskFirst  (void *p_arg);
static void SDCardTask();


/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*********************************************************************************************************
*/

int  main (void)
{
    OS_ERR  err;


    BSP_IntDisAll();                                            /* Disable all interrupts.                              */

    OSInit(&err);                                               /* Init uC/OS-III.                                      */
	
	// OSSchedRoundRobinCfg((CPU_BOOLEAN)DEF_TRUE, 
    //                      (OS_TICK    )10000,
    //                      (OS_ERR    *)&err);

    OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,                /* Create the start task                                */
                 (CPU_CHAR   *)"App Task Start",
                 (OS_TASK_PTR ) AppTaskStart,
                 (void       *) 0,
                 (OS_PRIO     ) APP_TASK_START_PRIO,
                 (CPU_STK    *)&AppTaskStartStk[0],
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE / 10,
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE,
                 (OS_MSG_QTY  ) 0,
                 (OS_TICK     ) 0,
                 (void       *) 0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

    OSStart(&err);                                              /* Start multitasking (i.e. give control to uC/OS-III). */
}


/*
*********************************************************************************************************
*                                          STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/

static  void  AppTaskStart (void *p_arg)
{
    CPU_INT32U  cpu_clk_freq;
    CPU_INT32U  cnts;
    OS_ERR      err;


   (void)p_arg;

    BSP_Init();                                                 /* Initialize BSP functions                             */
    CPU_Init();

    cpu_clk_freq = BSP_CPU_ClkFreq();                           /* Determine SysTick reference freq.                    */
    cnts = cpu_clk_freq / (CPU_INT32U)OSCfg_TickRate_Hz;        /* Determine nbr SysTick increments                     */
    OS_CPU_SysTickInit(cnts);                                   /* Init uC/OS periodic time src (SysTick).              */

    Mem_Init();                                                 /* Initialize Memory Management Module                  */

	#if (APP_CFG_SERIAL_EN == DEF_ENABLED)
		BSP_Ser_Init(115200);                                       /* Enable Serial Interface                              */
	#endif
    
    APP_TRACE_INFO(("Creating Application Tasks...\n\r"));
    AppTaskCreate();                                            /* Create Application Tasks                             */
    
    APP_TRACE_INFO(("Creating Application Object...\n\r"));
    AppObjCreate();                                             /* Create Application Objects                           */
    
    BSP_LED_On(0); // "Set the bit" turns off the LED (STM32F107VC board)
    while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */
        OSTimeDlyHMSM(0, 0, 0, 100,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
    }
}


/*
*********************************************************************************************************
*                                      CREATE APPLICATION TASKS
*
* Description:  This function creates the application tasks.
*
* Arguments  :  none
*
* Returns    :  none
*********************************************************************************************************
*/

static  void  AppTaskCreate (void)
{
	OS_ERR  err;
	
	OSTaskCreate((OS_TCB     *)&AppTaskFirstTCB, 
							 (CPU_CHAR   *)"App First Start",
							 (OS_TASK_PTR )AppTaskFirst,
							 (void       *)0,
							 (OS_PRIO     )APP_TASK_FIRST_PRIO,
							 (CPU_STK    *)&AppTaskFirstStk[0],
							 (CPU_STK_SIZE)APP_TASK_FIRST_STK_SIZE / 10,
							 (CPU_STK_SIZE)APP_TASK_FIRST_STK_SIZE,
							 (OS_MSG_QTY  )0,
							 (OS_TICK     )TASK_FIRST_RR_TIME_QUANTA,
							 (void       *)0,
							 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
							 (OS_ERR     *)&err);

	OSTaskCreate((OS_TCB     *) &SDCardTaskTCB, 
				(CPU_CHAR   *)"SDCard Task",
				(OS_TASK_PTR ) SDCardTask,
				(void       *) 0,
				(OS_PRIO     ) APP_TASK_FIRST_PRIO + 1,
				(CPU_STK    *) &SDCardTaskSTK[0],
				(CPU_STK_SIZE) APP_TASK_FIRST_STK_SIZE / 10,
				(CPU_STK_SIZE) APP_TASK_FIRST_STK_SIZE,
				(OS_MSG_QTY  ) 0,
				(OS_TICK     ) 0,
				(void       *) 0,
				(OS_OPT      ) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
				(OS_ERR     *) &err);
}


/*
*********************************************************************************************************
*                                      CREATE APPLICATION EVENTS
*
* Description:  This function creates the application kernel objects.
*
* Arguments  :  none
*
* Returns    :  none
*********************************************************************************************************
*/

static OS_ERR AppObjCreate (){
	OS_ERR err;

	OSMemCreate((OS_MEM		*) &MemPool,
				(CPU_CHAR	*) "MemPool",
				(void *		 ) &memPoolStorage[0],
				(OS_MEM_QTY	 ) MemPool_Size,
				(OS_MEM_SIZE ) 1000,
				(OS_ERR		*) &err);
	if(err != OS_ERR_NONE) return err;

	OSSemCreate((OS_SEM		*) &memPoolSem,
				(CPU_CHAR	*) "MemPoolSem",
				(OS_SEM_CTR	 ) MemPool_Size,
				(OS_ERR		*) &err);
	if(err != OS_ERR_NONE) return err;
}


/*
*********************************************************************************************************
*                                      AppTaskFirst()
*********************************************************************************************************
*/

static void AppTaskFirst (void *p_arg) {
	OS_ERR      err;
	
	int taskFirstCount = 0;
	while (DEF_TRUE) {
		taskFirstCount++;
		if (taskFirstCount % TASK_COUNT_PERIOD == 0) {
			BSP_LED_Toggle(1);
			USART_SendData(USART2, '*');
		}
	}
}

static void SDCardTask(){
	
}