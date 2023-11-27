/**
* @file tcpserver.c
* @brief A brief file description.
* @details
*     A more elaborated file description.
* @author Wang Mengyin
* @date 2010Jul31 21:00:58
* @note
*               Copyright 2010 Wang Mengyin.ALL RIGHTS RESERVED.
*                            http://tigerwang202.blogbus.com
*    This software is provided under license and contains proprietary and
* confidential material which is the property of Company Name tech.
*/


#define __TCPSERVER_C


/* Includes ------------------------------------------------------------------*/
#include "tcpserver.h"
#include "mem.h" /* Ϊ�˷����ڴ� */
#include <lwip/sockets.h> /* ʹ��BSD Socket�ӿڱ������sockets.h���ͷ�ļ� */
#include "includes.h"

#include "app_cfg.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#ifndef TCPSERVER_STK_SIZE          /* tcpserver ʹ�ö�ջ��С */
#define TCPSERVER_STK_SIZE  300
#endif

#ifndef LWIP_TCPSERVER_PRIO         /* tcpserver �������ȼ� */
#define LWIP_TCPSERVER_PRIO 15
#endif

#ifndef RECV_BUFFER_SIZE            /* ���ջ�������С */
#define RECV_BUFFER_SIZE    512
#endif

#ifndef TCPSERVER_PORT_NO
#define TCPSERVER_PORT_NO   5000    /* tcpserver ʹ�ö˿ں� */
#endif
/* Private macro -------------------------------------------------------------*/


/* Private variables ---------------------------------------------------------*/
static const char send_data[] = "This is TCP Server from uC/OS-II."; /* �����õ������� */
static OS_STK   TcpServerStack[TCPSERVER_STK_SIZE];

/* Private function prototypes -----------------------------------------------*/
static void tcpserv(void* parameter);
static void tcpserver_thread(void* arg);
/* Private functions ---------------------------------------------------------*/
/**
* @brief tcpserver accept tcp connect receive data and output on debug thermal.
* @param void* parameter :unused.
*/
static void tcpserv(void* parameter)
{
   char *recv_data; /* ���ڽ��յ�ָ�룬�������һ�ζ�̬��������������ڴ� */
   u32_t sin_size;
   int sock, connected, bytes_received;
   struct sockaddr_in server_addr, client_addr;
   bool stop = FALSE; /* ֹͣ��־ */

   recv_data = mem_malloc(RECV_BUFFER_SIZE); /* ��������õ����ݻ��� */
   if (recv_data == NULL)
   {
       printf("No memory\n");
       return;
   }

   /* һ��socket��ʹ��ǰ����ҪԤ�ȴ���������ָ��SOCK_STREAMΪTCP��socket */
   if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
       /* ����ʧ�ܵĴ����� */
       printf("Socket error\n");

       /* �ͷ��ѷ���Ľ��ջ��� */
       mem_free(recv_data);
       return;
   }

   /* ��ʼ������˵�ַ */
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(TCPSERVER_PORT_NO); /* ����˹����Ķ˿� */
   server_addr.sin_addr.s_addr = INADDR_ANY;
   memset(&(server_addr.sin_zero),8, sizeof(server_addr.sin_zero));

   /* ��socket������˵�ַ */
   if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
   {
       /* ��ʧ�� */
       printf("Unable to bind\n");

       /* �ͷ��ѷ���Ľ��ջ��� */
       mem_free(recv_data);
       return;
   }

   /* ��socket�Ͻ��м��� */
   if (listen(sock, 5) == -1)
   {
       printf("Listen error\n");

       /* release recv buffer */
       mem_free(recv_data);
       return;
   }

   printf("\nTCPServer Waiting for client on port %d...\n", TCPSERVER_PORT_NO);
   while(stop != TRUE)
   {
       sin_size = sizeof(struct sockaddr_in);

       /* ����һ���ͻ�������socket�����������������������ʽ�� */
       connected = accept(sock, (struct sockaddr *)&client_addr, &sin_size);
       /* ���ص������ӳɹ���socket */

       /* ���ܷ��ص�client_addrָ���˿ͻ��˵ĵ�ַ��Ϣ */
       printf("I got a connection from (%s , %d)\n",
                  inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

       /* �ͻ������ӵĴ��� */
       while (1)
       {
           /* �������ݵ�connected socket */
           //send(connected, send_data, strlen(send_data), 0);

           /* ��connected socket�н������ݣ�����buffer��1024��С��������һ���ܹ��յ�1024��С������ */
           bytes_received = recv(connected,recv_data, RECV_BUFFER_SIZE, 0);
           if (bytes_received <= 0)
           {
               if(bytes_received == 0)
                   printf("client close connection.\n");
               else
                   printf("received failed, server close connection.\n");
               
               /* ����ʧ�ܣ��ر����connected socket */
               lwip_close(connected);
               break;
           }

           /* �н��յ����ݣ���ĩ������ */
           recv_data[bytes_received] = '\0';
           if (strcmp(recv_data , "q") == 0 || strcmp(recv_data , "Q") == 0)
           {
               /* ���������ĸ��q��Q���ر�������� */
               printf("receive \"q\" command, close connection.\n");
               lwip_close(connected); // close socket
               break;
           }
           else if (strcmp(recv_data, "exit") == 0)
           {
               /* ������յ���exit����ر���������� */
               printf("receive \"exit\" command, exit tcpserver task.\n");
               lwip_close(connected); // close socket
               stop = TRUE;
               break;
           }
           else
           {
               /* �ڿ����ն���ʾ�յ������� */
               printf("RECIEVED DATA = %s \n" , recv_data);
           }
       } // end of while(1)
   } // end of while(stop != TRUE)

   /* �ͷŽ��ջ��� */
   mem_free(recv_data);

   return ;
}

/**
* @brief tcpserver thread
* @param void *arg :unused.
*/
static void tcpserver_thread(void *arg)
{
    tcpserv(arg);
    
    printf("Delete Tcpserver thread.\n");
    OSTaskDel(OS_PRIO_SELF);
}

/**
* @brief start tcpserver
*/
void tcpserver_init()
{
    CPU_INT08U  os_err;
    
    OSTaskCreate(tcpserver_thread, 
                 NULL, 
                 &TcpServerStack[TCPSERVER_STK_SIZE - 1], 
                 LWIP_TCPSERVER_PRIO
                     );
#if (OS_TASK_NAME_SIZE >= 11)
    OSTaskNameSet(LWIP_TCPSERVER_PRIO, (CPU_INT08U *)"tcpserver", &os_err);
#endif
}

/*-- File end --*/

