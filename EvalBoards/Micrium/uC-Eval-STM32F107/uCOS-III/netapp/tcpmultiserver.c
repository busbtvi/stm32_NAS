/**
* @file tcpmultiserver.c
* @brief A brief file description.
* @details
*     A more elaborated file description.
* @author Wang Mengyin
* @date 2010Aug2 16:00:25
* @note
*               Copyright 2010 Wang Mengyin.ALL RIGHTS RESERVED.
*                            http://tigerwang202.blogbus.com
*    This software is provided under license and contains proprietary and
* confidential material which is the property of Company Name tech.
*/


#define __TCPMULTISERVER_C


/* Includes ------------------------------------------------------------------*/
#include "tcpmultiserver.h"
#include "mem.h" /* Ϊ�˷����ڴ� */
#include <sockets.h>
#include "includes.h"

#include "app_cfg.h"

/* Private typedef -----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/
#ifndef TCPMULTISERVER_STK_SIZE          /* tcpserver ʹ�ö�ջ��С */
#define TCPMULTISERVER_STK_SIZE     300
#endif

#ifndef LWIP_TCPMULTISERVER_PRIO         /* tcpserver �������ȼ� */
#define LWIP_TCPMULTISERVER_PRIO    15
#endif

#define LWIP_TCPMULTISERVER_PORTNO  5000    /* �����������˿ں� */

/* Private macro -------------------------------------------------------------*/


/* Private variables ---------------------------------------------------------*/
static char send_data[] = "Hello Client[ ]\n"; /* �����õ������� */
static CPU_STK   TcpServerStack[TCPMULTISERVER_STK_SIZE];
static  OS_TCB   TcpServerTCB;

/* Private function prototypes -----------------------------------------------*/
static void tcpserv(void* parameter);
static void tcpmultiserver_thread(void* arg);

/* Private functions ---------------------------------------------------------*/
/**
* @brief tcpserver accept tcp connect receive data and output on debug thermal.
* @param void* parameter :unused.
*/
static void tcpserv(void* parameter)
{
   char *recv_data; /* ���ڽ��յ�ָ�룬�������һ�ζ�̬��������������ڴ� */
   u32_t sin_size;
   int listener, connected, bytes_received;
   struct sockaddr_in server_addr, client_addr;
   bool stop = FALSE; /* ֹͣ��־ */
   
   fd_set master; /* master file descriptor */
   fd_set read_fds; /* temp file descriptor list for select */
   int fdmax; /* maximum file descriptor number */
   int i, j;

   FD_ZERO(&master);    /* clear the master and temp sets */
   FD_ZERO(&read_fds);
   
   recv_data = mem_malloc(1024); /* ��������õ����ݻ��� */
   if (recv_data == NULL)
   {
       printf("No memory\n");
       return;
   }

   /* һ��socket��ʹ��ǰ����ҪԤ�ȴ���������ָ��SOCK_STREAMΪTCP��socket */
   if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
       /* ����ʧ�ܵĴ����� */
       printf("Socket error\n");

       /* �ͷ��ѷ���Ľ��ջ��� */
       mem_free(recv_data);
       return;
   }

   /* ��ʼ������˵�ַ */
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(LWIP_TCPMULTISERVER_PORTNO); /* ����˹����Ķ˿� */
   server_addr.sin_addr.s_addr = INADDR_ANY;
   memset(&(server_addr.sin_zero),8, sizeof(server_addr.sin_zero));

   /* ��socket������˵�ַ */
   if (bind(listener, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
   {
       /* ��ʧ�� */
       printf("Unable to bind\n");

       /* �ͷ��ѷ���Ľ��ջ��� */
       mem_free(recv_data);
       return;
   }

   /* ��socket�Ͻ��м��� */
   if (listen(listener, 5) == -1)
   {
       printf("Listen error\n");

       /* release recv buffer */
       mem_free(recv_data);
       return;
   }

   /* add the listener to the master set */
   FD_SET(listener, &master);

   /* keep track of the biggest file descriptor */
   fdmax = listener + 1;

   printf("\nTCPServer Waiting for client on port 5000...\n");
   while(stop != TRUE)
   {
       read_fds = master; /* copy it */
       /* block & waiting for connect or received data */
       if(lwip_select(fdmax, &read_fds, 0, 0, 0) == 0)
       {
           printf("select error.\n");
           continue;
       }
       
       for(i = 0; i < fdmax; i++)
       {
           if(FD_ISSET(i, &read_fds))
           { // we got one.
               if(i == listener)
               {   /* handle new connections */
                   sin_size = sizeof(struct sockaddr_in);
                   /* ����һ���ͻ�������socket�����������������������ʽ�� */
                   connected = accept(i, (struct sockaddr *)&client_addr, &sin_size);
                   /* ���ص������ӳɹ���socket */
                   if(connected == -1)
                       printf("accept connect error.\n");
                   else
                   {
                       FD_SET(connected, &master); /* add to master set */
                       if((connected + 1) > fdmax) /* keep track of the max */
                           fdmax = connected + 1;
                       /* ���ܷ��ص�client_addrָ���˿ͻ��˵ĵ�ַ��Ϣ */
                       printf("I got a connection from (%s , %d)\n",
                              inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                   }
               }
               else
               {    // handle data from a client
                   /* ��connected socket�н������ݣ�����buffer��1024��С��
                   \������һ���ܹ��յ�1024��С������ */
                   bytes_received = recv(i, recv_data, 1024, 0);
                   getpeername(i, (struct sockaddr *)&client_addr, &sin_size);
                   
                   if (bytes_received <= 0)
                   {
                       if(bytes_received < 0)
                           printf("received data failed, close client(%s, %d) connection.\n",
                                  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                       else
                           printf("disconnect client(%s, %d).\n", 
                                  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                       /* ����ʧ�ܣ��ر����connected socket */
                       lwip_close(i);
                       FD_CLR(i, &master); /* remove from master set */
                       continue;
                   }

                   /* �н��յ����ݣ���ĩ������ */
                   recv_data[bytes_received] = '\0';                   
                   if (strcmp(recv_data , "q") == 0 || strcmp(recv_data , "Q") == 0)
                   {
                       /* ���������ĸ��q��Q���ر�������� */
                       printf("receive \"q\" command, close client(%s, %d) connection.\n",
                              inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                       lwip_close(i);
                       FD_CLR(i, &master); /* remove from master set */
                   }
                   else if (strcmp(recv_data, "exit") == 0)
                   {
                       /* ������յ���exit����ر���������� */
                       printf("receive \"exit\" command, exit tcpserver task.\n");
                       stop = TRUE;
                   }
                   else if (strcmp(recv_data, "list") == 0)
                   {    /* ������յ���list���г������ӵĿͻ��� */
                       printf("Socket\tAddress\t\tPort\n");
                       for(j = 0; j < fdmax; j++)
                        if(FD_ISSET(j, &master) && j != listener)
                            printf("[%d]\t%s\t%d\n",
                                   j, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                   }
                   else
                   {    /* ���ػ�ӭ�ַ��� */
                       send_data[13] = i + '0'; /* Socket ��� */
                       send(i, send_data, sizeof(send_data) - 1, 0);
                        /* �ڿ����ն���ʾ�յ������� */
                       printf("RECIEVED DATA from client(%s, %d)\n%s\n",
                              inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), recv_data);
                   }
               } // End of Handle data from client
           } // End of new incoming connection
       } // end of loop through file descriptors
   } // end of while(stop != TRUE)

   
   /* �˳����� */
   for(i = 0; i < fdmax; i++)
       if(FD_ISSET(i, &master))
           lwip_close(i);
   
   /* clear the master and temp sets */
   FD_ZERO(&master);
   FD_ZERO(&read_fds);
   
   /* �ͷŽ��ջ��� */
   mem_free(recv_data);

   return ;
}

/**
* @brief tcpmultiserver thread
* @param void *arg :unused.
*/
static void tcpmultiserver_thread(void *arg)
{
    OS_ERR err3;
    tcpserv(arg);
    
    printf("Delete Tcpmultiserver thread.\n");
    OSTaskDel(&TcpServerTCB, &err3);
}

/**
* @brief start tcpmultiserver
*/
void tcpmultiserver_init()
{
    // CPU_INT08U  os_err;
    
    // OSTaskCreate(tcpmultiserver_thread, 
    //              NULL, 
    //              &TcpServerStack[TCPMULTISERVER_STK_SIZE - 1], 
    //              LWIP_TCPMULTISERVER_PRIO
    //                  );
    OS_ERR err3;

    OSTaskCreate((OS_TCB     *)&TcpServerTCB,
                (CPU_CHAR   *)"tcpmultiserver",
                (OS_TASK_PTR ) tcpmultiserver_thread,
                (void       *) 0,
                (OS_PRIO     ) LWIP_TCPMULTISERVER_PRIO,
                (CPU_STK    *) &TcpServerStack[0],
                (CPU_STK_SIZE) TCPMULTISERVER_STK_SIZE / 10,
                (CPU_STK_SIZE) TCPMULTISERVER_STK_SIZE,
                (OS_MSG_QTY  ) 0,
                (OS_TICK     ) 0,
                (void       *) 0,
                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                (OS_ERR     *)&err3);
}

/*-- File end --*/

