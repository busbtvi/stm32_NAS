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
#include "bsp_ser.h"
// #include <string.h>

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

volatile CPU_INT32U ADC_Value;

int isTest = 0;

typedef struct _linkedList {
	struct _linkedList *next;
	int len;
	char words[10];
} linkedList;
linkedList *head = NULL, *cur = NULL;

/*
*********************************************************************************************************
*                                              OS Objects
*********************************************************************************************************
*/

static int memPoolCnt = MemPool_Size;
static OS_MEM MemPool;
static char memPoolStorage[MemPool_Size][100];

static OS_TCB AppTaskStartTCB;
static OS_TCB AppTaskFirstTCB;
static OS_TCB cliTCB;
static OS_TCB volcanoDetectHandlerTCB;

static OS_FLAG_GRP sensorsFLAG;

/*
*********************************************************************************************************
*                                                STACKS
*********************************************************************************************************
*/

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  cliSTK[APP_TASK_FIRST_STK_SIZE];
static  CPU_STK  volcanoDetectHandlerSTK[256];


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskCreate (void);
static  OS_ERR AppObjCreate  (void);
static void SensorConfig(u32 RCC_APB2Periph, u16 GPIO_Pin, u8 GPIO_PortSource, u8 GPIO_PinSource, u32 EXTI_Line, u8 NVIC_IRQChannel, CPU_DATA int_id, CPU_FNCT_VOID handler);
static void UsartConfig();
static void ActionDetectISR();
static void usart2Handler();
static void volcanoDetectHandlerTask();

// TASK functions
static void AppTaskStart  (void *p_arg);
static void cliTask();

// Linked List functions
static void addNode();
static void deleteNode();

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

    AppObjCreate();                                             /* Create Application Objects                           */

	#if (APP_CFG_SERIAL_EN == DEF_ENABLED)
		BSP_Ser_Init(115200);                                       /* Enable Serial Interface                              */
	#endif

	UsartConfig();
	SensorConfig(
		RCC_APB2Periph_GPIOC,
		GPIO_Pin_4,
		GPIO_PortSourceGPIOC,
		GPIO_PinSource4,
		EXTI_Line4,
		EXTI4_IRQChannel,
		BSP_INT_ID_EXTI4,
		(CPU_FNCT_VOID) ActionDetectISR
	);
	SensorConfig(
		RCC_APB2Periph_GPIOB,
		GPIO_Pin_10,
		GPIO_PortSourceGPIOB,
		GPIO_PinSource10,
		EXTI_Line10,
		EXTI15_10_IRQChannel,
		BSP_INT_ID_EXTI15_10,
		(CPU_FNCT_VOID) ActionDetectISR
	);

	#if (APP_CFG_SERIAL_EN == DEF_ENABLED)
		BSP_Ser_Init(115200);                                       /* Enable Serial Interface                              */
	#endif
    

    APP_TRACE_INFO(("Creating Application Tasks...\n\r"));
    AppTaskCreate();                                            /* Create Application Tasks                             */
    
    // BSP_LED_On(1); // "Set the bit" turns off the LED (STM32F107VC board)
    while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */
        OSTimeDlyHMSM(0, 0, 1, 0,
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
	
	OSTaskCreate((OS_TCB     *) &cliTCB, 
				(CPU_CHAR   *) "cli",
				(OS_TASK_PTR ) cliTask,
				(void       *) 0,
				(OS_PRIO     ) 10,  // 적당히 낮게 설정한거(화산 감지보단 낮아야 하니)
				(CPU_STK    *) &cliSTK[0],
				(CPU_STK_SIZE) APP_TASK_FIRST_STK_SIZE / 10,
				(CPU_STK_SIZE) APP_TASK_FIRST_STK_SIZE,
				(OS_MSG_QTY  )0,
				(OS_TICK     )0,
				(void       *)0,
				(OS_OPT      ) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
				(OS_ERR     *) &err);

	OSTaskCreate((OS_TCB     *) &volcanoDetectHandlerTCB, 
				(CPU_CHAR   *) "volcanoDetectHandler",
				(OS_TASK_PTR ) volcanoDetectHandlerTask,
				(void       *) 0,
				(OS_PRIO     ) APP_TASK_FIRST_PRIO,
				(CPU_STK    *) &volcanoDetectHandlerSTK[0],
				(CPU_STK_SIZE) 256 / 10,
				(CPU_STK_SIZE) 256,
				(OS_MSG_QTY  )0,
				(OS_TICK     )0,
				(void       *)0,
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

	// OSSemCreate((OS_SEM		*) &memPoolSem,
	// 			(CPU_CHAR	*) "MemPoolSem",
	// 			(OS_SEM_CTR	 ) MemPool_Size,
	// 			(OS_ERR		*) &err);
	// if(err != OS_ERR_NONE) return err;

	OSFlagCreate((OS_FLAG_GRP*) &sensorsFLAG,
				(CPU_CHAR	 *) "SensorsFlag",
				(OS_FLAGS	  ) 0,
				(OS_ERR		 *) &err);

	return err;
}


/*
*********************************************************************************************************
*                                      Linked List Functions
*********************************************************************************************************
*/

static void addNode(){
	OS_ERR err;
	linkedList *newNode = OSMemGet((OS_MEM*) &MemPool, &err);
	if(err != OS_ERR_NONE){
		APP_TRACE_INFO(("MemPool is full\n\r"));
	}

	newNode->next = NULL;
	newNode->len = 0;

	if(head != NULL) cur->next = newNode;
	else head = newNode;

	cur = newNode;
}
static void deleteNode(){
	OS_ERR err;
	linkedList* del;

	while(head != NULL){
		del = head;
		head = head->next;

		OSMemPut((OS_MEM*) &MemPool, del, &err);
		if(err != OS_ERR_NONE){
			APP_TRACE_INFO(("MemPool is empty\n\r"));
			return;
		}
	}

	cur = NULL;
	head = NULL;
}

/*
*********************************************************************************************************
*                                      Handler Functions
*********************************************************************************************************
*/



/*
*********************************************************************************************************
*                                      AppTaskFirst()
*********************************************************************************************************
*/

static void cliTask(){
	OS_MSG_SIZE msgSize;
	OS_ERR err;
	linkedList *node;

	int cmpResult;
	char* cli = "test";

	while(DEF_TRUE){
		APP_TRACE_INFO(("Enter Command : "));
		node = OSTaskQPend((OS_TICK	 ) 0,
							(OS_OPT		 ) OS_OPT_PEND_BLOCKING,
							(OS_MSG_SIZE*) &msgSize,
							(CPU_TS		*) NULL,
							(OS_ERR		*) &err);
		
		if(err != OS_ERR_NONE){
			APP_TRACE_INFO(("OSTaskQPend Error\n\r"));
			continue;
		}

		// todo : cli 내용 여기에
		USART_SendData(USART2, '\n');
		while ((USART2->SR & USART_SR_TC) == 0);
		USART_SendData(USART2, '\r');
		while ((USART2->SR & USART_SR_TC) == 0);
		
		while(node != NULL){
			// for(int i=0; i<node->len; i++){
			// 	USART_SendData(USART2, node->words[i]);
			// 	while ((USART2->SR & USART_SR_TC) == 0);
			// }

			cmpResult = strncmp(cli, node->words, (node->len - 1));

			if(cmpResult == 0) USART_SendData(USART2, '0');
			else USART_SendData(USART2, '1');
			while ((USART2->SR & USART_SR_TC) == 0);

			node = node->next;
		}
		USART_SendData(USART2, '\n');
		while ((USART2->SR & USART_SR_TC) == 0);
		USART_SendData(USART2, '\r');
		while ((USART2->SR & USART_SR_TC) == 0);
		deleteNode();
	}
}
static void usart2Handler(){
	OS_ERR err;
	u16 word;

	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET){
		word = USART_ReceiveData(USART2) & 0xFF;
		
		if(word == '_'){
			OSTaskQPost((OS_TCB		*) &cliTCB,
						(void		*) head,
						(OS_MSG_SIZE ) 0,
						(OS_OPT		) OS_OPT_POST_FIFO,
						(OS_ERR		*) NULL);
		}
		else{
			if(cur == NULL || cur->len == 10) addNode();

			cur->words[cur->len++] = (char)word;
			USART_SendData(USART2, word);
		}

		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
		BSP_OS_SemPost(&BSP_SerRxWait);
	}
	
	FlagStatus tc_status = USART_GetFlagStatus(USART2, USART_FLAG_TC);
	if (tc_status == SET) {
		USART_ITConfig(USART2, USART_IT_TC, DISABLE);
		USART_ClearITPendingBit(USART2, USART_IT_TC);           /* Clear the USART2 receive interrupt.                */
		BSP_OS_SemPost(&BSP_SerTxWait);                         /* Post to the semaphore                              */
	}
}
static void volcanoDetectHandlerTask(){
	OS_ERR err;
	while(DEF_TRUE){
		OSFlagPend((OS_FLAG_GRP*) &sensorsFLAG,
					(OS_FLAGS	) 0x3,
					(OS_TICK	) 0,
					(OS_OPT		) OS_OPT_PEND_FLAG_SET_ALL,
					(CPU_TS	   *) NULL,
					(OS_ERR	   *) &err);

		USART_SendData(USART2, '!');
		// Todo : alert to user

		OSFlagPost((OS_FLAG_GRP*) &sensorsFLAG,
					(OS_FLAGS	) 0x3,
					(OS_OPT		) OS_OPT_POST_FLAG_CLR,
					(OS_ERR	   *) &err);
	}
}
void ActionDetectISR(){
	OS_ERR err;

	// button1
	if(EXTI_GetITStatus(EXTI_Line4) != RESET){
		// BSP_LED_Toggle(1);
		USART_SendData(USART2, '1');
		OSFlagPost((OS_FLAG_GRP*) &sensorsFLAG,
					(OS_FLAGS	) 0x1,
					(OS_OPT		) OS_OPT_POST_FLAG_SET,
					(OS_ERR	   *) &err);

		EXTI_ClearITPendingBit(EXTI_Line4);
	}
	// button2
	if(EXTI_GetITStatus(EXTI_Line10) != RESET){
		// BSP_LED_Toggle(2);
		USART_SendData(USART2, '2');
		OSFlagPost((OS_FLAG_GRP*) &sensorsFLAG,
					(OS_FLAGS	) 0x2,
					(OS_OPT		) OS_OPT_POST_FLAG_SET,
					(OS_ERR	   *) &err);

		EXTI_ClearITPendingBit(EXTI_Line10);
	}
}
void SensorConfig(u32 RCC_APB2Periph, u16 GPIO_Pin, u8 GPIO_PortSource, u8 GPIO_PinSource, u32 EXTI_Line, u8 NVIC_IRQChannel, CPU_DATA int_id, CPU_FNCT_VOID handler){
	// RCC
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph, ENABLE);

	// GPIO
	GPIO_InitTypeDef gpio = {
		.GPIO_Pin = GPIO_Pin,
		.GPIO_Mode = GPIO_Mode_IPU,
	};
	GPIO_Init(GPIOC, &gpio);

	// EXTI
	GPIO_EXTILineConfig(GPIO_PortSource, GPIO_PinSource);
	EXTI_InitTypeDef exti = {
		.EXTI_Line = EXTI_Line,
		.EXTI_Mode = EXTI_Mode_Interrupt,
		.EXTI_Trigger = EXTI_Trigger_Rising_Falling,
		.EXTI_LineCmd = ENABLE
	};
	EXTI_Init(&exti);

	// NVIC
	NVIC_InitTypeDef nvic = {
		.NVIC_IRQChannel = NVIC_IRQChannel,
		.NVIC_IRQChannelPreemptionPriority = 0x00,
		.NVIC_IRQChannelSubPriority = 0x00,
		.NVIC_IRQChannelCmd = ENABLE
	};
	NVIC_Init(&nvic);

	// Interrupt Handler
	BSP_IntVectSet(int_id, handler);
    BSP_IntEn(int_id);
}
void UsartConfig(){
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

   	USART_Cmd(USART2, ENABLE);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	
	NVIC_InitTypeDef nvic = {
		.NVIC_IRQChannel = USART2_IRQChannel,
		.NVIC_IRQChannelPreemptionPriority = 0x01,
		.NVIC_IRQChannelSubPriority = 0x01,
		.NVIC_IRQChannelCmd = ENABLE
	};
    NVIC_Init(&nvic);

	BSP_IntVectSet(BSP_INT_ID_USART2, usart2Handler);
	BSP_IntEn(BSP_INT_ID_USART2);
}