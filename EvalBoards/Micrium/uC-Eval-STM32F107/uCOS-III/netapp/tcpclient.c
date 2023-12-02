/**
* @file tcpclient.c
* @brief A brief file description.
* @details
*     A more elaborated file description.
* @author Wang Mengyin
* @date 2010Jul31 20:32:54
* @note
*               Copyright 2010 Wang Mengyin.ALL RIGHTS RESERVED.
*                            http://tigerwang202.blogbus.com
*    This software is provided under license and contains proprietary and
* confidential material which is the property of Company Name tech.
*/


#define __TCPCLIENT_C


/* Includes ------------------------------------------------------------------*/
#include "tcpclient.h"
//#include "opt.h"
#include "netdb.h" /* Ϊ�˽�������������Ҫ����netdb.hͷ�ļ� */
#include "sockets.h" /* ʹ��BSD Socket����Ҫ����sockets.hͷ�ļ� */
#include "mem.h" /* Ϊ�˷����ڴ� */
#include "includes.h"


/* Private typedef -----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/


/* Private macro -------------------------------------------------------------*/


/* Private variables ---------------------------------------------------------*/
static const char send_data[] = "This is TCP Client form uC/OS-II."; /* �����õ������� */

/* Private function prototypes -----------------------------------------------*/


/* Private functions ---------------------------------------------------------*/
void tcpclient(const char* url, int port)
{
    char *recv_data;
    // struct hostent *host;
    int sock, bytes_received;
    struct sockaddr_in server_addr;
    
       /* ͨ��������ڲ���url���host��ַ��������������������������� */
//    host = gethostbyname(url);

   /* �������ڴ�Ž������ݵĻ��� */
   recv_data = mem_malloc(1024);
   if (recv_data == NULL)
   {
       printf("No memory\n");
       return;
   }

   /* ����һ��socket��������SOCKET_STREAM��TCP���� */
   if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
       /* ����socketʧ�� */
       printf("Socket error\n");

       /* �ͷŽ��ջ��� */
       mem_free(recv_data);
       return;
   }

   /* ��ʼ��Ԥ���ӵķ���˵�ַ */
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(port);
//    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
   memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

   /* ���ӵ������ */
   if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
   {
       /* ����ʧ�� */
       printf("Connect error\n");
       
       /*�ͷŽ��ջ��� */
       mem_free(recv_data);
       return;
   }
   
   while(1)
   {
       /* ��sock�����н������1024�ֽ����� */
       bytes_received = recv(sock, recv_data, 1024, 0);
       if (bytes_received < 0)
       {
           /* ����ʧ�ܣ��ر�������� */
           lwip_close(sock);
           
           /* �ͷŽ��ջ��� */
           mem_free(recv_data);
           break;
       }
       
       /* �н��յ����ݣ���ĩ������ */
       recv_data[bytes_received] = '\0';
       
       if (strcmp(recv_data , "q") == 0 || strcmp(recv_data , "Q") == 0)
       {
           /* ���������ĸ��q��Q���ر�������� */
           lwip_close(sock);
           
           /* �ͷŽ��ջ��� */
           mem_free(recv_data);
           break;
       }
       else
       {
           /* �ڿ����ն���ʾ�յ������� */
           printf("\nRecieved data = %s " , recv_data);
       }
       
       /* �������ݵ�sock���� */
       send(sock,send_data,strlen(send_data), 0);
   }
   
   return;
}

/*-- File end --*/

