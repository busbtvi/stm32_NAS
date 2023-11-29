/**
* @file udpserver.c
* @brief A brief file description.
* @details
*     A more elaborated file description.
* @author Wang Mengyin
* @date 2010Jul31 21:32:57
* @note
*               Copyright 2010 Wang Mengyin.ALL RIGHTS RESERVED.
*                            http://tigerwang202.blogbus.com
*    This software is provided under license and contains proprietary and
* confidential material which is the property of Company Name tech.
*/


#define __UDPSERVER_C


/* Includes ------------------------------------------------------------------*/
#include "udpserver.h"
#include <sockets.h> /* ʹ��BSD socket����Ҫ����sockets.hͷ�ļ� */
#include <includes.h>
#include <string.h>
#include "mem.h"

#include "app_cfg.h"
/* Private typedef -----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/
#ifndef UDPSERVER_STK_SIZE          /* UDPSERVER ʹ�ö�ջ��С */
#define UDPSERVER_STK_SIZE  300
#endif

#ifndef LWIP_UDPSERVER_PRIO         /* UDPSERVER �������ȼ� */
#define LWIP_UDPSERVER_PRIO 16
#endif

/* Private macro -------------------------------------------------------------*/


/* Private variables ---------------------------------------------------------*/
static CPU_STK   UdpServerStack[UDPSERVER_STK_SIZE];
static  OS_TCB   UdpServerTCB;

/* Private function prototypes -----------------------------------------------*/
static void udpserv(void* paramemter);
static void udpserver_thread(void* arg);

/* Private functions ---------------------------------------------------------*/
static void udpserv(void* paramemter)
{
   int sock;
   int bytes_read;
   char *recv_data;
   u32_t addr_len;
   struct sockaddr_in server_addr, client_addr;

   /* ��������õ����ݻ��� */
   recv_data = mem_malloc(1024);
   if (recv_data == NULL)
   {
       /* �����ڴ�ʧ�ܣ����� */
       printf("No memory\n");
       return;
   }

   /* ����һ��socket��������SOCK_DGRAM��UDP���� */
   if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
   {
       printf("Socket error\n");

       /* �ͷŽ����õ����ݻ��� */
       mem_free(recv_data);
       return;
   }

   /* ��ʼ������˵�ַ */
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(5000);
   server_addr.sin_addr.s_addr = INADDR_ANY;
   memset(&(server_addr.sin_zero),0, sizeof(server_addr.sin_zero));

   /* ��socket������˵�ַ */
   if (bind(sock,(struct sockaddr *)&server_addr,
            sizeof(struct sockaddr)) == -1)
   {
       /* �󶨵�ַʧ�� */
       printf("Bind error\n");

       /* �ͷŽ����õ����ݻ��� */
       mem_free(recv_data);
       return;
   }

   addr_len = sizeof(struct sockaddr);
   printf("UDPServer Waiting for client on port 5000...\n");

   while (1)
   {
       /* ��sock����ȡ���1024�ֽ����� */
       bytes_read = recvfrom(sock, recv_data, 1024, 0,
                             (struct sockaddr *)&client_addr, &addr_len);
       /* UDP��ͬ��TCP�����������������ȡ������ʧ�ܵ���������������˳�ʱ�ȴ� */

       recv_data[bytes_read] = '\0'; /* ��ĩ������ */

       /* ������յ����� */
       printf("\n(%s , %d) said : ",inet_ntoa(client_addr.sin_addr),
                  ntohs(client_addr.sin_port));
       printf("%s", recv_data);

       /* �������������exit���˳� */
       if (strcmp(recv_data, "exit") == 0)
       {
           printf("receive \"exit\" command, exit udpserver task.\n");
           lwip_close(sock);

           /* �ͷŽ����õ����ݻ��� */
           mem_free(recv_data);
           break;
       }
   }

   return;
}

static void udpserver_thread(void* arg)
{
    OS_ERR err3;
    udpserv(arg);
    
    printf("Delete Udpserver thread.\n");
    OSTaskDel(&UdpServerTCB, &err3);
}


/**
* @brief start udpserver
*/
void udpserver_init()
{
    // CPU_INT08U  os_err;
    
    // OSTaskCreate(udpserver_thread, 
    //              NULL, 
    //              &UdpServerStack[UDPSERVER_STK_SIZE - 1], 
    //              LWIP_UDPSERVER_PRIO
    //                  );
    OS_ERR err3;

    OSTaskCreate((OS_TCB     *)&UdpServerTCB,
                (CPU_CHAR   *)"UDP Server",
                (OS_TASK_PTR ) udpserver_thread,
                (void       *) 0,
                (OS_PRIO     ) LWIP_UDPSERVER_PRIO,
                (CPU_STK    *) &UdpServerStack[0],
                (CPU_STK_SIZE) UDPSERVER_STK_SIZE / 10,
                (CPU_STK_SIZE) UDPSERVER_STK_SIZE,
                (OS_MSG_QTY  ) 0,
                (OS_TICK     ) 0,
                (void       *) 0,
                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                (OS_ERR     *)&err3);
}

/*-- File end --*/

