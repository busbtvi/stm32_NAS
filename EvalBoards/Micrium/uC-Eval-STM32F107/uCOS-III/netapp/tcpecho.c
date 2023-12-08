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
#include "tcpecho.h"
#include "os.h"
#include "opt.h"

#if LWIP_NETCONN

#include "sys.h"
#include "api.h"

#define TCP_ECHO_PORT   7
/*-----------------------------------------------------------------------------------*/

static OS_TCB tcpecho_threadTCB;
static CPU_STK tcpecho_threadStk[LWIP_STK_SIZE];

static void 
tcpecho_thread(void *arg)
{
  APP_TRACE_INFO(("tcpecho_thread: starting\n"));
  struct netconn *conn, *newconn;
  err_t err;
  LWIP_UNUSED_ARG(arg);

  /* Create a new connection identifier. */
  conn = netconn_new(NETCONN_TCP);

  /* Bind connection to well known port number TCP_ECHO_PORT. */
  netconn_bind(conn, IP_ADDR_ANY, TCP_ECHO_PORT);

  /* Tell connection to go into listening mode. */
  netconn_listen(conn);

  while (1) {

    /* Grab new connection. */
    newconn = netconn_accept(conn);
    printf("accepted new connection %p\n", newconn);
    /* Process the new connection. */
    if (newconn != NULL) {
      struct netbuf *buf;
      void *data;
      u16_t len;
      
      while ((buf = netconn_recv(newconn)) != NULL) {
        printf("Recved\n");
        do {
             netbuf_data(buf, &data, &len);
             err = netconn_write(newconn, data, len, NETCONN_COPY);
#if 0
            if (err != ERR_OK) {
              printf("tcpecho: netconn_write: error \"%s\"\n", lwip_strerr(err));
            }
#endif
        } while (netbuf_next(buf) >= 0);
        netbuf_delete(buf);
      }
      printf("Got EOF, looping\n"); 
      /* Close connection and discard connection identifier. */
      netconn_close(newconn);
      netconn_delete(newconn);
    }
  }
}
/*-----------------------------------------------------------------------------------*/
void tcpecho_init(void){
    OS_ERR err3;
    // sys_thread_new("tcpecho", tcpecho_thread, NULL, LWIP_STK_SIZE, 2);
    APP_TRACE_INFO(("tcpecho_init: OSTaskCreate\n"));
    OSTaskCreate((OS_TCB     *)&tcpecho_threadTCB,
                (CPU_CHAR   *) "tcpecho",
                (OS_TASK_PTR ) tcpecho_thread,
                (void       *) NULL,
                (OS_PRIO     ) LWIP_START_PRIO + 2 - 1,
                (CPU_STK    *) &tcpecho_threadStk[0],
                (CPU_STK_SIZE) LWIP_STK_SIZE / 10,
                (CPU_STK_SIZE) LWIP_STK_SIZE,
                (OS_MSG_QTY  ) 0,
                (OS_TICK     ) 0,
                (void       *) 0,
                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                (OS_ERR     *) &err3);
    if(err3 != OS_ERR_NONE){
      APP_TRACE_INFO(("tcpecho_init: OSTaskCreate failed %d\n", err3));
      return;
    }
}
/*-----------------------------------------------------------------------------------*/

#endif /* LWIP_NETCONN */
