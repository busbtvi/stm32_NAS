/**
 * @file
 * Sequential API Main thread module
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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

#include "opt.h"

#if !NO_SYS /* don't build if not configured for use in lwipopts.h */

#include "sys.h"
#include "memp.h"
#include "pbuf.h"
#include "ip_frag.h"
#include "tcp.h"
#include "autoip.h"
#include "dhcp.h"
#include "igmp.h"
#include "dns.h"
#include "tcpip.h"
#include "init.h"
#include "etharp.h"
#include "ppp_oe.h"
#include "os.h"

/* global variables */
static void (* tcpip_init_done)(void *arg);
static void *tcpip_init_done_arg;
static OS_Q mbox;

#if LWIP_TCPIP_CORE_LOCKING
/** The global semaphore to lock the stack. */
sys_sem_t lock_tcpip_core;
#endif /* LWIP_TCPIP_CORE_LOCKING */

#if LWIP_TCP
/* global variable that shows if the tcp timer is currently scheduled or not */
// static int tcpip_tcp_timer_active;
static char    tcpip_tcp_timerCreated = 0;
static OS_TCB  TcpIpThreadTCB;
static CPU_STK TcpIpThreadSTK[TCPIP_THREAD_STACKSIZE];

static OS_TCB   tcpip_tcp_timerTCB;
static CPU_STK  tcpip_tcp_timerSTK[128];

#define TimeTaskPrio 5
#if IP_REASSEMBLY
static OS_TCB ip_reass_timerTCB;
static CPU_STK ip_reass_timerSTK[128];
#endif /* IP_REASSEMBLY */
#if LWIP_ARP
static OS_TCB arp_timerTCB;
static CPU_STK arp_timerSTK[128];
#endif /* LWIP_ARP */
#if LWIP_DHCP
static OS_TCB dhcp_timer_coarseTCB;
static CPU_STK dhcp_timer_coarseSTK[128];
static OS_TCB dhcp_timer_fineTCB;
static CPU_STK dhcp_timer_fineSTK[128];
#endif /* LWIP_DHCP */
#if LWIP_AUTOIP
static OS_TCB autoip_timerTCB;
static CPU_STK autoip_timerSTK[128];
#endif /* LWIP_AUTOIP */
#if LWIP_IGMP
static OS_TCB igmp_timerTCB;
static CPU_STK igmp_timerSTK[128];
#endif /* LWIP_IGMP */
#if LWIP_DNS
static OS_TCB dns_timerTCB;
static CPU_STK dns_timerSTK[128];
#endif /* LWIP_DNS */

/**
 * Timer callback function that calls tcp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void tcpip_tcp_timer(void *arg){
  // LWIP_UNUSED_ARG(arg);

  // /* call TCP timer handler */
  // tcp_tmr();
  // /* timer still needed? */
  // if (tcp_active_pcbs || tcp_tw_pcbs) {
  //   /* restart timer */
  //   sys_timeout(TCP_TMR_INTERVAL, tcpip_tcp_timer, NULL);
  // } else {
  //   /* disable timer */
  //   tcpip_tcp_timer_active = 0;
  // }
  OS_ERR err3;

  while(DEF_ON){
    tcp_tmr();
    if (tcp_active_pcbs || tcp_tw_pcbs) {
      OSTimeDlyHMSM(0, 0, 0, 250, OS_OPT_TIME_PERIODIC, &err3);
    } else {
      /* disable timer */
      OSTaskSuspend(&tcpip_tcp_timerTCB, &err3);
    }
  }

}

#if !NO_SYS
/**
 * Called from TCP_REG when registering a new PCB:
 * the reason is to have the TCP timer only running when
 * there are active (or time-wait) PCBs.
 */
void tcp_timer_needed(){
  // /* timer is off but needed again? */
  // if (!tcpip_tcp_timer_active && (tcp_active_pcbs || tcp_tw_pcbs)) {
  //   /* enable and start timer */
  //   tcpip_tcp_timer_active = 1;
  //   sys_timeout(TCP_TMR_INTERVAL, tcpip_tcp_timer, NULL);
  // }
  OS_ERR err3 = OS_ERR_NONE;

  if(tcp_active_pcbs || tcp_tw_pcbs){
    if(tcpip_tcp_timerCreated == 0){
      OSTaskCreate(&tcpip_tcp_timerTCB, "tcpip_tcp_timer", tcpip_tcp_timer, 0, TimeTaskPrio,
        &tcpip_tcp_timerSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
      
      if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("tcpip_tcp_timer: OSTaskCreate failed %d\n\r", err3));
      }
      else tcpip_tcp_timerCreated = 1;
    }
    else{
      OSTaskResume(&tcpip_tcp_timerTCB, &err3);
      
      if(err3 != OS_ERR_NONE){
        APP_TRACE_INFO(("tcpip_tcp_timer: OSTaskResume failed %d\n\r", err3));
      }
    }
  }
}
#endif /* !NO_SYS */
#endif /* LWIP_TCP */

#if IP_REASSEMBLY
/**
 * Timer callback function that calls ip_reass_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
ip_reass_timer(void *arg)
{
  // LWIP_UNUSED_ARG(arg);
  // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip: ip_reass_tmr()\n"));
  // ip_reass_tmr();
  // sys_timeout(IP_TMR_INTERVAL, ip_reass_timer, NULL);
  OS_ERR err3 = OS_ERR_NONE;
  while(DEF_ON){
    OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_PERIODIC, &err3);
    ip_reass_tmr();

    if(err3 != OS_ERR_NONE)
      APP_TRACE_INFO(("ip_reass_timer: OSTimeDlyHMSM failed %d\n\r", err3));
  }
}
#endif /* IP_REASSEMBLY */

#if LWIP_ARP
/**
 * Timer callback function that calls etharp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
arp_timer(void *arg)
{
  // LWIP_UNUSED_ARG(arg);
  // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip: etharp_tmr()\n"));
  // etharp_tmr();
  // sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
  OS_ERR err3 = OS_ERR_NONE;
  while(DEF_ON){
    etharp_tmr();
    OSTimeDlyHMSM(0, 0, 5, 0, OS_OPT_TIME_PERIODIC, &err3);

    if(err3 != OS_ERR_NONE)
      APP_TRACE_INFO(("arp_timer: OSTimeDlyHMSM failed %d\n\r", err3));
  }
}
#endif /* LWIP_ARP */

#if LWIP_DHCP
/**
 * Timer callback function that calls dhcp_coarse_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
dhcp_timer_coarse(void *arg)
{
  // LWIP_UNUSED_ARG(arg);
  // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip: dhcp_coarse_tmr()\n"));
  // dhcp_coarse_tmr();
  // sys_timeout(DHCP_COARSE_TIMER_MSECS, dhcp_timer_coarse, NULL);
  OS_ERR err3 = OS_ERR_NONE;
  while(DEF_ON){
    dhcp_coarse_tmr();
    OSTimeDlyHMSM(0, 1, 0, 0, OS_OPT_TIME_PERIODIC, &err3);

    if(err3 != OS_ERR_NONE)
      APP_TRACE_INFO(("dhcp_timer_coarse: OSTimeDlyHMSM failed %d\n\r", err3));
  }
}

/**
 * Timer callback function that calls dhcp_fine_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
dhcp_timer_fine(void *arg)
{
  // LWIP_UNUSED_ARG(arg);
  // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip: dhcp_fine_tmr()\n"));
  // dhcp_fine_tmr();
  // sys_timeout(DHCP_FINE_TIMER_MSECS, dhcp_timer_fine, NULL);
  OS_ERR err3 = OS_ERR_NONE;
  while(DEF_ON){
    dhcp_fine_tmr();
    OSTimeDlyHMSM(0, 0, 0, 500, OS_OPT_TIME_PERIODIC, &err3);

    if(err3 != OS_ERR_NONE)
      APP_TRACE_INFO(("dhcp_timer_fine: OSTimeDlyHMSM failed %d\n\r", err3));
  }
}
#endif /* LWIP_DHCP */

#if LWIP_AUTOIP
/**
 * Timer callback function that calls autoip_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
autoip_timer(void *arg)
{
  // LWIP_UNUSED_ARG(arg);
  // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip: autoip_tmr()\n"));
  // autoip_tmr();
  // sys_timeout(AUTOIP_TMR_INTERVAL, autoip_timer, NULL);
  OS_ERR err3 = OS_ERR_NONE;
  while(DEF_ON){
    autoip_tmr();
    OSTimeDlyHMSM(0, 0, 0, 100, OS_OPT_TIME_PERIODIC, &err3);

    if(err3 != OS_ERR_NONE)
      APP_TRACE_INFO(("autoip_timer: OSTimeDlyHMSM failed %d\n\r", err3));
  }
}
#endif /* LWIP_AUTOIP */

#if LWIP_IGMP
/**
 * Timer callback function that calls igmp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
igmp_timer(void *arg)
{
  // LWIP_UNUSED_ARG(arg);
  // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip: igmp_tmr()\n"));
  // igmp_tmr();
  // sys_timeout(IGMP_TMR_INTERVAL, igmp_timer, NULL);
  OS_ERR err3 = OS_ERR_NONE;
  while(DEF_ON){
    igmp_tmr();
    OSTimeDlyHMSM(0, 0, 0, 100, OS_OPT_TIME_PERIODIC, &err3);

    if(err3 != OS_ERR_NONE)
      APP_TRACE_INFO(("igmp_timer: OSTimeDlyHMSM failed %d\n\r", err3));
  }
}
#endif /* LWIP_IGMP */

#if LWIP_DNS
/**
 * Timer callback function that calls dns_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
dns_timer(void *arg)
{
  // LWIP_UNUSED_ARG(arg);
  // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip: dns_tmr()\n"));
  // dns_tmr();
  // sys_timeout(DNS_TMR_INTERVAL, dns_timer, NULL);
  OS_ERR err3 = OS_ERR_NONE;
  while(DEF_ON){
    dns_tmr();
    OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_PERIODIC, &err3);

    if(err3 != OS_ERR_NONE)
      APP_TRACE_INFO(("dns_timer: OSTimeDlyHMSM failed %d\n\r", err3));
  }
}
#endif /* LWIP_DNS */

/**
 * The main lwIP thread. This thread has exclusive access to lwIP core functions
 * (unless access to them is not locked). Other threads communicate with this
 * thread using message boxes.
 *
 * It also starts all the timers to make sure they are running in the right
 * thread context.
 *
 * @param arg unused argument
 */
static void
tcpip_thread(void *arg)
{
  struct tcpip_msg *msg;
  OS_ERR err3;
  LWIP_UNUSED_ARG(arg);
  APP_TRACE_INFO(("tcpip_thread\n\r"));

#if IP_REASSEMBLY
  OSTaskCreate(&ip_reass_timerTCB, "ip_reass_timer", ip_reass_timer, 0, TimeTaskPrio,
    &ip_reass_timerSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("tcpip_thread ip_reass_timer error\n\r"));
  }
  // sys_timeout(IP_TMR_INTERVAL, ip_reass_timer, NULL);
#endif /* IP_REASSEMBLY */
#if LWIP_ARP
  OSTaskCreate(&arp_timerTCB, "arp_timer", arp_timer, 0, TimeTaskPrio,
    &arp_timerSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("tcpip_thread arp_timer error\n\r"));
  }
  // sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
#endif /* LWIP_ARP */
#if LWIP_DHCP
  OSTaskCreate(&dhcp_timer_coarseTCB, "dhcp_timer_coarse", dhcp_timer_coarse, 0, TimeTaskPrio,
    &dhcp_timer_coarseSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("tcpip_thread dhcp_timer_coarse error\n\r"));
  }
  // sys_timeout(DHCP_COARSE_TIMER_MSECS, dhcp_timer_coarse, NULL);
  OSTaskCreate(&dhcp_timer_fineTCB, "dhcp_timer_fine", dhcp_timer_fine, 0, TimeTaskPrio,
    &dhcp_timer_fineSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("tcpip_thread dhcp_timer_fine error\n\r"));
  }
  // sys_timeout(DHCP_FINE_TIMER_MSECS, dhcp_timer_fine, NULL);
#endif /* LWIP_DHCP */
#if LWIP_AUTOIP
  OSTaskCreate(&autoip_timerTCB, "autoip_timer", autoip_timer, 0, TimeTaskPrio,
    &autoip_timerSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("tcpip_thread autoip_timer error\n\r"));
  }
  // sys_timeout(AUTOIP_TMR_INTERVAL, autoip_timer, NULL);
#endif /* LWIP_AUTOIP */
#if LWIP_IGMP
  OSTaskCreate(&igmp_timerTCB, "igmp_timer", igmp_timer, 0, TimeTaskPrio,
    &igmp_timerSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("tcpip_thread igmp_timer error\n\r"));
  }
  // sys_timeout(IGMP_TMR_INTERVAL, igmp_timer, NULL);
#endif /* LWIP_IGMP */
#if LWIP_DNS
  OSTaskCreate(&dns_timerTCB, "dns_timer", dns_timer, 0, TimeTaskPrio,
    &dns_timerSTK[0], 12, 128, 0, 0, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("tcpip_thread dns_timer error\n\r"));
  }
  // sys_timeout(DNS_TMR_INTERVAL, dns_timer, NULL);
#endif /* LWIP_DNS */

  if (tcpip_init_done != NULL) {
    tcpip_init_done(tcpip_init_done_arg);
  }
  APP_TRACE_INFO(("264\n\r"));

  LOCK_TCPIP_CORE();
  while (1) {                          /* MAIN Loop */
    APP_TRACE_INFO(("before sys_mbox_fetch\n\r"));
    sys_mbox_fetch(&mbox, (void *)&msg);
    APP_TRACE_INFO(("after sys_mbox_fetch\n\r"));
    switch (msg->type) {
#if LWIP_NETCONN
    case TCPIP_MSG_API:
      LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip_thread: API message %p\n", (void *)msg));
      msg->msg.apimsg->function(&(msg->msg.apimsg->msg));
      break;
#endif /* LWIP_NETCONN */

    case TCPIP_MSG_INPKT:
      LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip_thread: PACKET %p\n", (void *)msg));
#if LWIP_ARP
      if (msg->msg.inp.netif->flags & NETIF_FLAG_ETHARP) {
        ethernet_input(msg->msg.inp.p, msg->msg.inp.netif);
      } else
#endif /* LWIP_ARP */
      { ip_input(msg->msg.inp.p, msg->msg.inp.netif);
      }
      memp_free(MEMP_TCPIP_MSG_INPKT, msg);
      break;

#if LWIP_NETIF_API
    case TCPIP_MSG_NETIFAPI:
      LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip_thread: Netif API message %p\n", (void *)msg));
      msg->msg.netifapimsg->function(&(msg->msg.netifapimsg->msg));
      break;
#endif /* LWIP_NETIF_API */

    case TCPIP_MSG_CALLBACK:
      LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip_thread: CALLBACK %p\n", (void *)msg));
      msg->msg.cb.f(msg->msg.cb.ctx);
      memp_free(MEMP_TCPIP_MSG_API, msg);
      break;

    case TCPIP_MSG_TIMEOUT:
      APP_TRACE_INFO(("tcpip_thread: TIMEOUT %p\n", (void *)msg));
      // LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip_thread: TIMEOUT %p\n", (void *)msg));
      // sys_timeout(msg->msg.tmo.msecs, msg->msg.tmo.h, msg->msg.tmo.arg);
      // memp_free(MEMP_TCPIP_MSG_API, msg);
      break;
    case TCPIP_MSG_UNTIMEOUT:
      LWIP_DEBUGF(TCPIP_DEBUG, ("tcpip_thread: UNTIMEOUT %p\n", (void *)msg));
      sys_untimeout(msg->msg.tmo.h, msg->msg.tmo.arg);
      memp_free(MEMP_TCPIP_MSG_API, msg);
      break;

    default:
      break;
    }
  }
}

/**
 * Pass a received packet to tcpip_thread for input processing
 *
 * @param p the received packet, p->payload pointing to the Ethernet header or
 *          to an IP header (if netif doesn't got NETIF_FLAG_ETHARP flag)
 * @param inp the network interface on which the packet was received
 */
err_t
tcpip_input(struct pbuf *p, struct netif *inp)
{
  struct tcpip_msg *msg;

  if (sys_mbox_valid(&mbox)) {
    msg = memp_malloc(MEMP_TCPIP_MSG_INPKT);
    if (msg == NULL) {  
      return ERR_MEM;
    }

    msg->type = TCPIP_MSG_INPKT;
    msg->msg.inp.p = p;
    msg->msg.inp.netif = inp;
    if (sys_mbox_trypost(&mbox, msg) != ERR_OK) {
      memp_free(MEMP_TCPIP_MSG_INPKT, msg);
      return ERR_MEM;
    }
    return ERR_OK;
  }
  return ERR_VAL;
}

/**
 * Call a specific function in the thread context of
 * tcpip_thread for easy access synchronization.
 * A function called in that way may access lwIP core code
 * without fearing concurrent access.
 *
 * @param f the function to call
 * @param ctx parameter passed to f
 * @param block 1 to block until the request is posted, 0 to non-blocking mode
 * @return ERR_OK if the function was called, another err_t if not
 */
err_t
tcpip_callback_with_block(void (*f)(void *ctx), void *ctx, u8_t block)
{
  struct tcpip_msg *msg;

    if (sys_mbox_valid(&mbox)) {
    msg = memp_malloc(MEMP_TCPIP_MSG_API);
    if (msg == NULL) {
      return ERR_MEM;
    }

    msg->type = TCPIP_MSG_CALLBACK;
    msg->msg.cb.f = f;
    msg->msg.cb.ctx = ctx;
    if (block) {
      sys_mbox_post(&mbox, msg);
    } else {
      if (sys_mbox_trypost(&mbox, msg) != ERR_OK) {
        memp_free(MEMP_TCPIP_MSG_API, msg);
        return ERR_MEM;
      }
    }
    return ERR_OK;
  }
  return ERR_VAL;
}

/**
 * call sys_timeout in tcpip_thread
 *
 * @param msec time in miliseconds for timeout
 * @param h function to be called on timeout
 * @param arg argument to pass to timeout function h
 * @return ERR_MEM on memory error, ERR_OK otherwise
 */
err_t
tcpip_timeout(u32_t msecs, sys_timeout_handler h, void *arg)
{
  struct tcpip_msg *msg;

  if (sys_mbox_valid(&mbox)) {
    msg = memp_malloc(MEMP_TCPIP_MSG_API);
    if (msg == NULL) {
      return ERR_MEM;
    }

    msg->type = TCPIP_MSG_TIMEOUT;
    msg->msg.tmo.msecs = msecs;
    msg->msg.tmo.h = h;
    msg->msg.tmo.arg = arg;
    sys_mbox_post(&mbox, msg);
    return ERR_OK;
  }
  return ERR_VAL;
}

/**
 * call sys_untimeout in tcpip_thread
 *
 * @param msec time in miliseconds for timeout
 * @param h function to be called on timeout
 * @param arg argument to pass to timeout function h
 * @return ERR_MEM on memory error, ERR_OK otherwise
 */
err_t
tcpip_untimeout(sys_timeout_handler h, void *arg)
{
  struct tcpip_msg *msg;

  if (sys_mbox_valid(&mbox)) {
    msg = memp_malloc(MEMP_TCPIP_MSG_API);
    if (msg == NULL) {
      return ERR_MEM;
    }

    msg->type = TCPIP_MSG_UNTIMEOUT;
    msg->msg.tmo.h = h;
    msg->msg.tmo.arg = arg;
    sys_mbox_post(&mbox, msg);
    return ERR_OK;
  }
  return ERR_VAL;
}

#if LWIP_NETCONN
/**
 * Call the lower part of a netconn_* function
 * This function is then running in the thread context
 * of tcpip_thread and has exclusive access to lwIP core code.
 *
 * @param apimsg a struct containing the function to call and its parameters
 * @return ERR_OK if the function was called, another err_t if not
 */
err_t
tcpip_apimsg(struct api_msg *apimsg)
{
  struct tcpip_msg msg;
  
  if (sys_mbox_valid(&mbox)) {
    msg.type = TCPIP_MSG_API;
    msg.msg.apimsg = apimsg;
    sys_mbox_post(&mbox, &msg);
    sys_arch_sem_wait(apimsg->msg.conn->op_completed, 0);
    return ERR_OK;
  }
  return ERR_VAL;
}

#if LWIP_TCPIP_CORE_LOCKING
/**
 * Call the lower part of a netconn_* function
 * This function has exclusive access to lwIP core code by locking it
 * before the function is called.
 *
 * @param apimsg a struct containing the function to call and its parameters
 * @return ERR_OK (only for compatibility fo tcpip_apimsg())
 */
err_t
tcpip_apimsg_lock(struct api_msg *apimsg)
{
  LOCK_TCPIP_CORE();
  apimsg->function(&(apimsg->msg));
  UNLOCK_TCPIP_CORE();
  return ERR_OK;

}
#endif /* LWIP_TCPIP_CORE_LOCKING */
#endif /* LWIP_NETCONN */

#if LWIP_NETIF_API
#if !LWIP_TCPIP_CORE_LOCKING
/**
 * Much like tcpip_apimsg, but calls the lower part of a netifapi_*
 * function.
 *
 * @param netifapimsg a struct containing the function to call and its parameters
 * @return error code given back by the function that was called
 */
err_t
tcpip_netifapi(struct netifapi_msg* netifapimsg)
{
  struct tcpip_msg msg;
  
  if (sys_mbox_valid(&mbox)) {
    netifapimsg->msg.sem = sys_sem_new(0);
    if (netifapimsg->msg.sem == SYS_SEM_NULL) {
      netifapimsg->msg.err = ERR_MEM;
      return netifapimsg->msg.err;
    }
    
    msg.type = TCPIP_MSG_NETIFAPI;
    msg.msg.netifapimsg = netifapimsg;
    sys_mbox_post(&mbox, &msg);
    APP_TRACE_INFO(("need to do sys_sem_wait\n\r"));
    // sys_sem_wait(netifapimsg->msg.sem);
    sys_sem_free(netifapimsg->msg.sem);
    netifapimsg->msg.sem = NULL;
    return netifapimsg->msg.err;
  }
  return ERR_VAL;
}
#else /* !LWIP_TCPIP_CORE_LOCKING */
/**
 * Call the lower part of a netifapi_* function
 * This function has exclusive access to lwIP core code by locking it
 * before the function is called.
 *
 * @param netifapimsg a struct containing the function to call and its parameters
 * @return ERR_OK (only for compatibility fo tcpip_netifapi())
 */
err_t
tcpip_netifapi_lock(struct netifapi_msg* netifapimsg)
{
  LOCK_TCPIP_CORE();  
  netifapimsg->function(&(netifapimsg->msg));
  UNLOCK_TCPIP_CORE();
  return netifapimsg->msg.err;
}
#endif /* !LWIP_TCPIP_CORE_LOCKING */
#endif /* LWIP_NETIF_API */

/**
 * Initialize this module:
 * - initialize all sub modules
 * - start the tcpip_thread
 *
 * @param initfunc a function to call when tcpip_thread is running and finished initializing
 * @param arg argument to pass to initfunc
 */
void
tcpip_init(void (* initfunc)(void *), void *arg)
{
  OS_ERR err3;
  lwip_init();

  tcpip_init_done = initfunc;
  tcpip_init_done_arg = arg;
  // mbox = sys_mbox_new(TCPIP_MBOX_SIZE);
  OSQCreate(&mbox, "TCPIP MBOX", TCPIP_MBOX_SIZE, &err3);
  APP_TRACE_INFO(("OSQCreate = %d\n\r", err3));
#if LWIP_TCPIP_CORE_LOCKING
  lock_tcpip_core = sys_sem_new(1);
#endif /* LWIP_TCPIP_CORE_LOCKING */

  // sys_thread_new(TCPIP_THREAD_NAME, tcpip_thread, NULL, TCPIP_THREAD_STACKSIZE, TCPIP_THREAD_PRIO);
  OSTaskCreate((OS_TCB     *)&TcpIpThreadTCB,
              (CPU_CHAR   *) "TCPIP Thread",
              (OS_TASK_PTR ) tcpip_thread,
              (void       *) NULL,
              (OS_PRIO     ) LWIP_START_PRIO + TCPIP_THREAD_PRIO - 1,
              (CPU_STK    *) &TcpIpThreadSTK[0],
              (CPU_STK_SIZE) TCPIP_THREAD_STACKSIZE / 10,
              (CPU_STK_SIZE) TCPIP_THREAD_STACKSIZE,
              (OS_MSG_QTY  ) 0,
              (OS_TICK     ) 0,
              (void       *) 0,
              (OS_OPT      ) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
              (OS_ERR     *) &err3);
  APP_TRACE_INFO(("OSTaskCreate done\n\r"));
}

/**
 * Simple callback function used with tcpip_callback to free a pbuf
 * (pbuf_free has a wrong signature for tcpip_callback)
 *
 * @param p The pbuf (chain) to be dereferenced.
 */
static void
pbuf_free_int(void *p)
{
  struct pbuf *q = p;
  pbuf_free(q);
}

/**
 * A simple wrapper function that allows you to free a pbuf from interrupt context.
 *
 * @param p The pbuf (chain) to be dereferenced.
 * @return ERR_OK if callback could be enqueued, an err_t if not
 */
err_t
pbuf_free_callback(struct pbuf *p)
{
  return tcpip_callback_with_block(pbuf_free_int, p, 0);
}

/**
 * A simple wrapper function that allows you to free heap memory from
 * interrupt context.
 *
 * @param m the heap memory to free
 * @return ERR_OK if callback could be enqueued, an err_t if not
 */
err_t
mem_free_callback(void *m)
{
  return tcpip_callback_with_block(mem_free, m, 0);
}

#endif /* !NO_SYS */
