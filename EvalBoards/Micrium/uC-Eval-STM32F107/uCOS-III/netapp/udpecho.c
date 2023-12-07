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

#include "udpecho.h"
#include "os.h"
#include "opt.h"

#if LWIP_NETCONN

#include "api.h"
#include "sys.h"

#define UDP_ECHO_PORT   7

/*-----------------------------------------------------------------------------------*/

OS_TCB udpecho_threadTCB;
CPU_STK udpecho_threadSTK[LWIP_STK_SIZE];

static void
udpecho_thread(void *arg)
{
  static struct netconn *conn;
  static struct netbuf *buf;
  static struct ip_addr *addr;
  static unsigned short port;
  char buffer[256];

  LWIP_UNUSED_ARG(arg);

  conn = netconn_new(NETCONN_UDP); /* Create a new netconnect */
  LWIP_ASSERT("con != NULL", conn != NULL);
  netconn_bind(conn, IP_ADDR_ANY, UDP_ECHO_PORT); /* udpecho using port 7 */

  while (1) {
    buf = netconn_recv(conn); /* received data to buffer */
    if (buf != NULL) {
      addr = netbuf_fromaddr(buf); /* get client's IP address */
      port = netbuf_fromport(buf); /* get client's port */
      netconn_connect(conn, addr, port); /* connect to client */
      netbuf_copy(buf, buffer, buf->p->tot_len); /* set echo data */
      buffer[buf->p->tot_len] = '\0';
      buf->addr = NULL;
      netconn_send(conn, buf); /* send data back to client */
      LWIP_DEBUGF(LWIP_DBG_ON, ("got %s\n", buffer));
      netbuf_delete(buf); /* release buffer */
    }
  }
}
/*-----------------------------------------------------------------------------------*/
void
udpecho_init(void)
{ /* Create 1 uC/OS-II Task Priority = LWIP_START_PRIO + (2 - 1) */  
  // sys_thread_new("udpecho", udpecho_thread, NULL, LWIP_STK_SIZE, 2);
  OS_ERR err3;
  OSTaskCreate((OS_TCB     *)&udpecho_threadTCB,
                (CPU_CHAR   *) "udpecho",
                (OS_TASK_PTR ) udpecho_thread,
                (void       *) NULL,
                (OS_PRIO     ) LWIP_START_PRIO + 2 - 1,
                (CPU_STK    *) &udpecho_threadSTK[0],
                (CPU_STK_SIZE) LWIP_STK_SIZE / 10,
                (CPU_STK_SIZE) LWIP_STK_SIZE,
                (OS_MSG_QTY  ) 0,
                (OS_TICK     ) 0,
                (void       *) 0,
                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                (OS_ERR     *) &err3);

  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("udpecho_init: OSTaskCreate err %d\n", err3));
  }
}

#endif /* LWIP_NETCONN */
