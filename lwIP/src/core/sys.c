/**
 * @file
 * lwIP Operating System abstraction
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

#if (NO_SYS == 0) /* don't build if not configured for use in lwipopts.h */

#include "sys.h"
#include "def.h"
#include "memp.h"
#include "tcpip.h"

/** The one and only timeout list */
static struct sys_timeo *next_timeout;

/**
 * Struct used for sys_sem_wait_timeout() to tell wether the time
 * has run out or the semaphore has really become available.
 */
struct sswt_cb
{
  s16_t timeflag;
  OS_SEM *psem;
};

/**
 * Wait (forever) for a message to arrive in an mbox.
 * While waiting, timeouts (for this thread) are processed.
 *
 * @param mbox the mbox to fetch the message from
 * @param msg the place to store the message
 */
void
sys_mbox_fetch(OS_Q* mbox, void **msg)
{
  OS_ERR	err3;
  OS_MSG_SIZE   msg_size;
  CPU_TS        in_timeout = 1;

  *msg  = OSQPend(mbox, in_timeout, OS_OPT_PEND_BLOCKING, &msg_size, NULL, &err3);
  if(err3 != OS_ERR_NONE){
    APP_TRACE_INFO(("sys_mbox_fetch: OSQPend err %d\n", err3));
  }
}


/**
 * Go through timeout list (for this task only) and remove the first matching
 * entry, even though the timeout has not triggered yet.
 *
 * @note This function only works as expected if there is only one timeout
 * calling 'h' in the list of timeouts.
 *
 * @param h callback function that would be called by the timeout
 * @param arg callback argument that would be passed to h
*/
void
sys_untimeout(sys_timeout_handler h, void *arg)
{
  struct sys_timeo *prev_t, *t;

  if (next_timeout == NULL) {
    return;
  }

  for (t = next_timeout, prev_t = NULL; t != NULL; prev_t = t, t = t->next) {
    if ((t->h == h) && (t->arg == arg)) {
      /* We have a match */
      /* Unlink from previous in list */
      if (prev_t == NULL) {
        next_timeout = t->next;
      } else {
        prev_t->next = t->next;
      }
      /* If not the last one, add time of this one back to next */
      if (t->next != NULL) {
        t->next->time += t->time;
      }
      memp_free(MEMP_SYS_TIMEOUT, t);
      return;
    }
  }
  return;
}

#endif /* NO_SYS */