/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#define SYS_ARCH_GLOBALS

#include "string.h"
#include "os_cfg_app.h"
/* lwIP includes. */
#include "debug.h"
#include "def.h"
#include "sys.h"
#include "mem.h"

#include "include/arch/sys_arch.h"

static OS_MEM pQueueMem;

const void * const pvNullPointer = (mem_ptr_t*)0xffffffff;

static char pcQueueMemoryPool[MAX_QUEUES][sizeof(TQ_DESCR) + sizeof(void*)];
// static char pcQueueMemoryPool[MAX_QUEUES * sizeof(TQ_DESCR) + MEM_ALIGNMENT - 1];

//SYS_ARCH_EXT OS_STK LWIP_TASK_STK[LWIP_TASK_MAX][LWIP_STK_SIZE];
#define NewThreadMax  1
static  OS_TCB   TcpServerTCB[NewThreadMax];
static  int      availableTcbIndex = 0;

/* Message queue constants. */
#define archMESG_QUEUE_LENGTH	( 6 )
#define archPOST_BLOCK_TIME_MS	( ( unsigned portLONG ) 10000 )

struct sys_timeouts lwip_timeouts[LWIP_TASK_MAX];
struct sys_timeouts null_timeouts;

/*-----------------------------------------------------------------------------------*/
//  Creates an empty mailbox.
int sys_mbox_new(OS_Q* mbox, int size){
    OS_ERR       ucErr;
      
    OSQCreate(mbox,"LWIP quiue", size, &ucErr); 
    LWIP_ASSERT( "OSQCreate ", ucErr == OS_ERR_NONE );
    
    if( ucErr == OS_ERR_NONE){ 
        return 0; 
    }
    return -1;
    // prarmeter "size" can be ignored in your implementation.
    // u8_t       ucErr;
    // PQ_DESCR    pQDesc;
    // OS_ERR err3;
    
    // pQDesc = (PQ_DESCR) OSMemGet(&pQueueMem, &err3);
    // LWIP_ASSERT("OSMemGet ", err3 == OS_ERR_NONE );
    
    // if( err3 == OS_ERR_NONE ) 
    // {
    //     if( size > MAX_QUEUE_ENTRIES ) // �����������MAX_QUEUE_ENTRIES��Ϣ��Ŀ
    //         size = MAX_QUEUE_ENTRIES;
        
    //     // pQDesc->pQ = OSQCreate( &(pQDesc->pvQEntries[0]), size ); 
    //     OSQCreate ((OS_Q      *)  pQDesc->pvQEntries[0],
    //                 (CPU_CHAR  *) "sys_mbox_new",
    //                 (OS_MSG_QTY ) size,
    //                 (OS_ERR    *) &err3);
    //     LWIP_ASSERT( "OSQCreate ", pQDesc->pQ != NULL );
        
    //     if( pQDesc->pQ != NULL ) 
    //         return pQDesc; 
    //     else
    //     {
    //         // ucErr = OSMemPut( pQueueMem, pQDesc );
    //         OSMemPut(&pQueueMem, pQDesc , &err3);
    //         return SYS_MBOX_NULL;
    //     }
    // }
    // else return SYS_MBOX_NULL;
}

/*-----------------------------------------------------------------------------------*/
/*
  Deallocates a mailbox. If there are messages still present in the
  mailbox when the mailbox is deallocated, it is an indication of a
  programming error in lwIP and the developer should be notified.
*/
void
sys_mbox_free(OS_Q* mbox)
{
    OS_ERR     ucErr;
    LWIP_ASSERT( "sys_mbox_free ", sys_mbox_valid(mbox) );      
        
    OSQFlush(mbox,& ucErr);
    
    OSQDel(mbox, OS_OPT_DEL_ALWAYS, &ucErr);
    LWIP_ASSERT( "OSQDel ", ucErr == OS_ERR_NONE );
}

/*-----------------------------------------------------------------------------------*/
//   Posts the "msg" to the mailbox.
void
sys_mbox_post(OS_Q* mbox, void *msg)
{
    OS_ERR     err3;
    CPU_INT08U  i=0;
    if(msg == NULL) msg = (void*) &pvNullPointer;
  
    /* try 10 times */
    while(i<10){
        OSQPost(mbox, msg, 0, OS_OPT_POST_ALL, &err3);
        if(err3 == OS_ERR_NONE) break;
        i++;
        OSTimeDly(5, OS_OPT_TIME_DLY, &err3);
    }
    LWIP_ASSERT( "sys_mbox_post error!\n", i !=10 );  
}

// Try to post the "msg" to the mailbox.
err_t sys_mbox_trypost(OS_Q* mbox, void *msg)
{
    OS_ERR     err3;

    if(msg == NULL) msg = (void*) &pvNullPointer;  
    
    OSQPost(mbox, msg, 0, OS_OPT_POST_ALL, &err3);    
    
    if(err3 != OS_ERR_NONE)
        return ERR_MEM;
    return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/*
  Blocks the thread until a message arrives in the mailbox, but does
  not block the thread longer than "timeout" milliseconds (similar to
  the sys_arch_sem_wait() function). The "msg" argument is a result
  parameter that is set by the function (i.e., by doing "*msg =
  ptr"). The "msg" parameter maybe NULL to indicate that the message
  should be dropped.

  The return values are the same as for the sys_arch_sem_wait() function:
  Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
  timeout.

  Note that a function with a similar name, sys_mbox_fetch(), is
  implemented by lwIP. 
*/
u32_t sys_arch_mbox_fetch(OS_Q* mbox, void **msg, u32_t timeout)
{ 
    OS_ERR	err3;
    OS_MSG_SIZE   msg_size;
    CPU_TS        ucos_timeout;  
    CPU_TS        in_timeout = (timeout * OS_CFG_TICK_RATE_HZ)/1000;

    if(timeout && in_timeout == 0) in_timeout = 1;

    *msg  = OSQPend((OS_Q       *) mbox,
                    (OS_TICK     ) in_timeout,
                    (OS_OPT      ) OS_OPT_PEND_BLOCKING,
                    (OS_MSG_SIZE*) &msg_size,
                    (CPU_TS     *) &ucos_timeout,
                    (OS_ERR     *) &err3);

    if ( err3 == OS_ERR_TIMEOUT ) 
        ucos_timeout = SYS_ARCH_TIMEOUT;  
    return ucos_timeout; 
}

/** 
  * Check if an mbox is valid/allocated: 
  * @param sys_mbox_t *mbox pointer mail box
  * @return 1 for valid, 0 for invalid 
  */ 
int sys_mbox_valid(OS_Q* mbox){
    if(mbox->NamePtr)  
        return (strcmp(mbox->NamePtr,"?Q"))? 1:0;
    return 0;
}
/** 
  * Set an mbox invalid so that sys_mbox_valid returns 0 
  */      
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
  if(sys_mbox_valid(mbox))
    sys_mbox_free(mbox);
}

/*-----------------------------------------------------------------------------------*/
//  Creates and returns a new semaphore. The "count" argument specifies
//  the initial state of the semaphore. TBD finish and test
err_t sys_sem_new(OS_SEM* sem, u8_t count){
    OS_ERR err3;

    // pSem = OSSemCreate((u16_t)count);
    OSSemCreate((OS_SEM     *) sem,
                (CPU_CHAR   *) "new_sem",
                (OS_SEM_CTR  ) count,
                (OS_ERR     *) &err3);

    if(err3 != OS_ERR_NONE) return -1;    
    return 0;
}

/*-----------------------------------------------------------------------------------*/
/*
  Blocks the thread while waiting for the semaphore to be
  signaled. If the "timeout" argument is non-zero, the thread should
  only be blocked for the specified time (measured in
  milliseconds).

  If the timeout argument is non-zero, the return value is the number of
  milliseconds spent waiting for the semaphore to be signaled. If the
  semaphore wasn't signaled within the specified time, the return value is
  SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
  (i.e., it was already signaled), the function may return zero.

  Notice that lwIP implements a function with a similar name,
  sys_sem_wait(), that uses the sys_arch_sem_wait() function.
*/
u32_t
sys_arch_sem_wait(OS_SEM* sem, u32_t timeout)
{ 
    OS_ERR err3;
    CPU_TS ucos_timeout;
    CPU_TS in_timeout = (timeout * OS_CFG_TICK_RATE_HZ) / 1000;
    
    if(timeout && in_timeout == 0) in_timeout = 1;

    OSSemPend((OS_SEM   *) sem,
        (OS_TICK     ) in_timeout,
        (OS_OPT      ) OS_OPT_PEND_BLOCKING,
        (CPU_TS     *) &ucos_timeout,
        (OS_ERR     *) &err3);
    
    /*  only when timeout! */
    if(err3 == OS_ERR_TIMEOUT)
      ucos_timeout = SYS_ARCH_TIMEOUT;	
    return ucos_timeout;
}

/*-----------------------------------------------------------------------------------*/
// Signals a semaphore
void
sys_sem_signal(OS_SEM* sem)
{
    // u8_t ucErr;
    // ucErr = OSSemPost((OS_EVENT *)sem);
    OS_ERR err3;

    OSSemPost((OS_SEM *)sem, (OS_OPT) OS_OPT_POST_1, (OS_ERR *) &err3);
    
    // May be called when a connection is already reset, should not check...
    // LWIP_ASSERT( "OSSemPost ", ucErr == OS_ERR_NONE );
}

/*-----------------------------------------------------------------------------------*/
// Deallocates a semaphore
void
sys_sem_free(OS_SEM* sem)
{
    // u8_t     ucErr;
    // (void)OSSemDel( (OS_EVENT *)sem, OS_DEL_ALWAYS, &ucErr );
    OS_ERR err3;
    OSSemDel((OS_SEM *)sem, (OS_OPT) OS_OPT_DEL_ALWAYS, (OS_ERR *) &err3);
    LWIP_ASSERT( "OSSemDel ", err3 == OS_ERR_NONE );
}
int sys_sem_valid(OS_SEM* sem){
  if(sem->NamePtr)
    return (strcmp(sem->NamePtr,"?SEM"))? 1:0;
  else
    return 0;
}
/** Set a semaphore invalid so that sys_sem_valid returns 0 */
void sys_sem_set_invalid(OS_SEM* sem){
  if(sys_sem_valid(sem))
    sys_sem_free(sem);
}

/*-----------------------------------------------------------------------------------*/
// Initialize sys arch
void
sys_init(void)
{
    OS_ERR err3;
	u8_t i;
        
        // init mem used by sys_mbox_t, use ucosII functions
        // ָ���ڴ���ʼ��ַ��4�ֽڶ���
        // pQueueMem = OSMemCreate( 
        //                         (void*)((u32_t)((u32_t)pcQueueMemoryPool+MEM_ALIGNMENT-1) & ~(MEM_ALIGNMENT-1)), 
        //                         MAX_QUEUES, sizeof(TQ_DESCR), &ucErr 
        //                             );
        // OSMemGet((OS_MEM   *) pQueueMem,
        //         (OS_ERR    *) &err3);
        OSMemCreate((OS_MEM    *) &pQueueMem,
                   (CPU_CHAR   *) "pQueueMem",
                   (void       *) &pcQueueMemoryPool[0],
                   (OS_MEM_QTY  ) MAX_QUEUES,
                   (OS_MEM_SIZE ) sizeof(TQ_DESCR) + sizeof(void *),
                   (OS_ERR     *) &err3);
        
        LWIP_ASSERT( "OSMemCreate ", err3 == OS_ERR_NONE);

	// Initialize the the per-thread sys_timeouts structures
	// make sure there are no valid pids in the list
	for(i = 0; i < LWIP_TASK_MAX; i++)
	{
            lwip_timeouts[i].next = NULL;
	}
}

/*-----------------------------------------------------------------------------------*/
/*
  Returns a pointer to the per-thread sys_timeouts structure. In lwIP,
  each thread has a list of timeouts which is represented as a linked
  list of sys_timeout structures. The sys_timeouts structure holds a
  pointer to a linked list of timeouts. This function is called by
  the lwIP timeout scheduler and must not return a NULL value. 

  In a single threaded sys_arch implementation, this function will
  simply return a pointer to a global sys_timeouts variable stored in
  the sys_arch module.
*/
struct sys_timeouts *
sys_arch_timeouts(void)
{
    u8_t curr_prio;
    s8_t ubldx;

    // OS_TCB curr_task_pcb;
    
    null_timeouts.next = NULL;
    // OSTaskQuery(OS_PRIO_INIT,&curr_task_pcb);
    curr_prio = OSTCBCurPtr->Prio;

    ubldx = (u8_t)(curr_prio - LWIP_START_PRIO);
    
    if((ubldx>=0) && (ubldx<LWIP_TASK_MAX))
    {
    	//printf("\nlwip_timeouts[%d],prio=%d!!! \n",ubldx,curr_prio);
    	return &lwip_timeouts[ubldx];
    }
    else
    {
    	//printf("\nnull_timeouts,prio=%d!!! \n",curr_prio);
    	return &null_timeouts;
    }
}

/*-----------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
// TBD 
/*-----------------------------------------------------------------------------------*/
/*
  Starts a new thread with priority "prio" that will begin its execution in the
  function "thread()". The "arg" argument will be passed as an argument to the
  thread() function. The id of the new thread is returned. Both the id and
  the priority are system dependent.
*/
sys_thread_t sys_thread_new(char *name, void (* thread)(void *arg), void *arg, int stacksize, int prio)
{
    u8_t ubPrio = 0;
    u8_t ucErr;
    OS_ERR err3;
    
    arg = arg;
    
    if((prio > 0) && (prio <= LWIP_TASK_MAX))
    {
        ubPrio = (u8_t)(LWIP_START_PRIO + prio - 1);

        if(stacksize > LWIP_STK_SIZE)   // �����ջ��С������LWIP_STK_SIZE
            stacksize = LWIP_STK_SIZE;
        
        // OSTaskCreate(thread, (void *)arg, &LWIP_TASK_STK[prio-1][stacksize-1],ubPrio);
        OSTaskCreate((OS_TCB     *)&TcpServerTCB[availableTcbIndex],
                    (CPU_CHAR   *) name,
                    (OS_TASK_PTR ) thread,
                    (void       *) arg,
                    (OS_PRIO     ) ubPrio,
                    (CPU_STK    *) &LWIP_TASK_STK[prio-1][0],
                    (CPU_STK_SIZE) stacksize / 10,
                    (CPU_STK_SIZE) stacksize,
                    (OS_MSG_QTY  ) 0,
                    (OS_TICK     ) 0,
                    (void       *) 0,
                    (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                    (OS_ERR     *)&err3);
        availableTcbIndex++;
        // OSTaskCreateExt(thread, (void *)arg, &LWIP_TASK_STK[prio-1][stacksize-1],ubPrio
        //                 ,ubPrio,&LWIP_TASK_STK[prio-1][0],stacksize,(void *)0,OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    }
        return ubPrio;
}


