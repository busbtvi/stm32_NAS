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

volatile CPU_INT32U ADC_Value;

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

/*
*********************************************************************************************************
*                                                STACKS
*********************************************************************************************************
*/

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  AppTaskFirstStk[APP_TASK_FIRST_STK_SIZE];


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskCreate (void);
static  OS_ERR AppObjCreate  (void);
static  void  AppTaskStart  (void *p_arg);
static  void  AppTaskFirst  (void *p_arg);

static void SensorConfig(u32 RCC_APB2Periph, u16 GPIO_Pin, u8 GPIO_PortSource, u8 GPIO_PinSource, u32 EXTI_Line, u8 NVIC_IRQChannel, CPU_DATA int_id, CPU_FNCT_VOID handler);
void EXTI4_handler();

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
	SensorConfig(
		RCC_APB2Periph_GPIOC,
		GPIO_Pin_4,
		GPIO_PortSourceGPIOC,
		GPIO_PinSource4,
		EXTI_Line4,
		EXTI4_IRQChannel,
		BSP_INT_ID_EXTI4,
		(void*)EXTI4_handler
	);

	#if (APP_CFG_SERIAL_EN == DEF_ENABLED)
		BSP_Ser_Init(115200);                                       /* Enable Serial Interface                              */
	#endif
    
    APP_TRACE_INFO(("Creating Application Tasks...\n\r"));
    AppTaskCreate();                                            /* Create Application Tasks                             */
    
    APP_TRACE_INFO(("Creating Application Object...\n\r"));
    AppObjCreate();                                             /* Create Application Objects                           */
    
    BSP_LED_On(1); // "Set the bit" turns off the LED (STM32F107VC board)
    while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */
        OSTimeDlyHMSM(0, 0, 1, 0,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
		BSP_LED_Toggle(1);
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
				(OS_TICK     )0,
				(void       *)0,
				(OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
				(OS_ERR     *)&err);
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
	return err;
}


/*
*********************************************************************************************************
*                                      AppTaskFirst()
*********************************************************************************************************
*/

static void AppTaskFirst (void *p_arg) {
	OS_ERR err;
	u16 temp;
	while (DEF_TRUE) {
		BSP_LED_Toggle(2);
		OSTimeDlyHMSM(0, 0, 3, 0,
					OS_OPT_TIME_HMSM_STRICT,
					&err);

		// USART_SendData(USART2, ADC_Value);
		//      4294967295
		int i = 1000000000, j = 0;
		for(j=0; j<9; j++){
			USART_SendData(USART2, (u16)(ADC_Value/i + '0'));
			while ((USART2->SR & USART_SR_TC) == 0);
			i = i/10;
		}
		// USART_SendData(USART2, (u16)(ADC_Value & 0xffff));
		// USART_SendData(USART2, '*');
		USART_SendData(USART2, '\n');
		while ((USART2->SR & USART_SR_TC) == 0);
		USART_SendData(USART2, '\r');
		while ((USART2->SR & USART_SR_TC) == 0);
		// USART_SendData(USART2, '\r');
	}
}
void EXTI4_handler(){
	if(EXTI_GetITStatus(EXTI_Line4) != RESET){
		BSP_LED_Toggle(3);
		USART_SendData(USART2, '*');
		
		EXTI_ClearITPendingBit(EXTI_Line4);
	}
}
void SensorConfig(u32 RCC_APB2Periph, u16 GPIO_Pin, u8 GPIO_PortSource, u8 GPIO_PinSource, u32 EXTI_Line, u8 NVIC_IRQChannel, CPU_DATA int_id, CPU_FNCT_VOID handler){
	// RCC
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph, ENABLE);

	// GPIO
	GPIO_InitTypeDef button1_gpio = {
		.GPIO_Pin = GPIO_Pin,
		.GPIO_Mode = GPIO_Mode_IPU,
	};
	GPIO_Init(GPIOC, &button1_gpio);

	// EXTI
	GPIO_EXTILineConfig(GPIO_PortSource, GPIO_PinSource);
	EXTI_InitTypeDef button1_exti = {
		.EXTI_Line = EXTI_Line,
		.EXTI_Mode = EXTI_Mode_Interrupt,
		.EXTI_Trigger = EXTI_Trigger_Rising_Falling,
		.EXTI_LineCmd = ENABLE
	};
	EXTI_Init(&button1_exti);

	// NVIC
	NVIC_InitTypeDef button1_nvic = {
		.NVIC_IRQChannel = NVIC_IRQChannel,
		.NVIC_IRQChannelPreemptionPriority = 0x00,
		.NVIC_IRQChannelSubPriority = 0x00,
		.NVIC_IRQChannelCmd = ENABLE
	};

	// Interrupt Handler
	BSP_IntVectSet(int_id, handler);
    BSP_IntEn(int_id);
}