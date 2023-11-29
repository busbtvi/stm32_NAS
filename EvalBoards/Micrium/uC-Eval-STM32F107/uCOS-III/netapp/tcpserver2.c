/**
* @file tcpserver2.c
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


#define __TCPSERVER2_C


/* Includes ------------------------------------------------------------------*/
#include "tcpserver2.h"
#include "mem.h" /* Ϊ�˷����ڴ� */
#include <sockets.h>
#include "includes.h"

#include "netdatapack.h" /* Ϊ��ʵ����������ת�� */
//#include "stm3210c_eval_lcd.h"

#include "app_cfg.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#ifndef TCPSERVER2_STK_SIZE          /* tcpserver ʹ�ö�ջ��С */
#define TCPSERVER2_STK_SIZE  300
#endif

#ifndef LWIP_TCPSERVER2_PRIO         /* tcpserver �������ȼ� */
#define LWIP_TCPSERVER2_PRIO 15
#endif

#ifndef RECV_BUFFER_SIZE            /* ���ջ�������С */
#define RECV_BUFFER_SIZE    512
#endif

#ifndef TCPSERVER2_PORT_NO
#define TCPSERVER2_PORT_NO   5011    /* tcpserver ʹ�ö˿ں� */
#endif

#define CUSTOM_HEADER_LEN       5       /* ���ݰ�ͷ���� */

#define CUSTOM_MAGIC_NUM        0x0711  /* ���� :) */

#define CUSTOM_MAGIC_OFFSET     0       /* ���ݰ�������ƫ�Ƶ�ַ */
#define CUSTOM_DATA_LEN_OFFSET  2       /* ���ݰ����ȵ�ƫ�Ƶ�ַ */
#define CUSTOM_CMD_OFFSET       4       /* ���ݰ������ƫ�Ƶ�ַ */


#define CUSTOM_CMD_LED          1       /* ����LED״̬ */
#define CUSTOM_CMD_ADC          2       /* ��ȡADC״̬ */
#define CUSTOM_CMD_LCD          3       /* ����LCD״̬ */
#define CUSTOM_CMD_ECHO         4       /* ��ȡEcho��Ϣ */ 

#define CUSTOM_ADC_LEN          2       /* ����ADC���ݰ����� */
#define CUSTOM_LCD_LEN          (16 * 4 + 1) /* ����LCD���ݰ�����*/
#define CUSTOM_ECHO_LEN         (strlen(send_data) + 2) /* ����echo���ݰ����� */
/* Private macro -------------------------------------------------------------*/
#define CUSTOM_GET_LENGTH(a)    unpacki16(((unsigned char* )(a)) + CUSTOM_DATA_LEN_OFFSET)

/* Private variables ---------------------------------------------------------*/
static const char send_data[] = "This is TCP Server from uC/OS-II."; /* �����õ������� */
static CPU_STK   TcpServerStack[TCPSERVER2_STK_SIZE];
static  OS_TCB   TcpServerTCB;

/* Private function prototypes -----------------------------------------------*/
static void tcpserv(void* parameter);
static void tcpserver_thread(void* arg);
static int blocking_lwip_recv(int socket, void* mem, int len);
/* Private functions ---------------------------------------------------------*/

/**
* @fn static int processCMD(
*         unsigned char* buf
*         )
* @brief parse received data.
* @param unsigned char* buf :a pointer to received data buffer.
* @return int return send data length.
*/
static int processCMD(unsigned char* buf)
{
    int send_len = 0;
    u16 magic_num, body_length, adc;
    u8 cmd, led, lcd_dispbuf[CUSTOM_LCD_LEN];
    unpack(buf, "hhc", &magic_num, &body_length, &cmd); // ��ȡͷ��Ϣ
    
#if 1
    // debug
    printf("Custom TCP data header:\n");
    printf("Byte[0 1] : magic number = 0x%04X\n", magic_num);
    printf("Byte[2 3] : body length  = 0x%04X\n", body_length);
    printf("Byte[4  ] : command      = 0x%04X\n", cmd);
#endif
    
    switch(cmd)
    {
    case CUSTOM_CMD_LED:
        {
            unpack(buf + CUSTOM_HEADER_LEN, "c", &led); // ��ȡLCD״̬
            if(led != 0)
                BSP_LED_On(2); /* led 2 on */
            else
                BSP_LED_Off(2); /* led 2 off */
            printf("set led[2] %s\n", led != 0 ? "ON": "OFF");
        }
        break;
    case CUSTOM_CMD_ADC:
        {
            adc = BSP_ADC_GetStatus(1); // ��ȡADCֵ
            // ���ɷ������ݰ�
            send_len = pack(buf, "hhch", 
                            CUSTOM_MAGIC_NUM,  /* Byte[0 1]    magic number */
                            CUSTOM_ADC_LEN,    /* Byte[2 3]    length       */
                            CUSTOM_CMD_ADC,    /* Byte[4  ]    command      */
                            adc                /* Byte[5 6]    adc data     */);
            printf("send ADC value: %d\n", adc);
        }
        break;
    case CUSTOM_CMD_LCD:
        {
            unpack(buf + CUSTOM_HEADER_LEN, "s", &lcd_dispbuf); // ��ȡLCD���ַ�
            LCD_Clear();
            LCD_Puts(lcd_dispbuf); /* show received string in 128*64 lcd */
            printf("lcd display: %s \n", lcd_dispbuf);
        }
        break;
    case CUSTOM_CMD_ECHO:
        {
            // ���û����ַ���
            send_len = pack(buf + CUSTOM_HEADER_LEN, "s", send_data);
            // д�����ݰ�ͷ
            send_len += pack(buf, "hhc",
                            CUSTOM_MAGIC_NUM,  /* Byte[0 1]  magic number */
                            send_len,          /* Byte[2 3]  length       */
                            CUSTOM_CMD_ECHO    /* Byte[4  ]  command      */);
            printf("send echo string: %s \n", send_data);
        }
        break;
    default:
        {   /* undefined CMD, I don't think that will happen. */
            printf("error unknown command interpret.\n");
        }
    }
    return send_len;
}

/**
* @fn static int blocking_lwip_recv(
*         int socket,
*          void* mem,
*          int len
*         )
* @brief receive certain length of data form socket, note functon will block until
* given data length is received.
* @param int socket :socket.
* @param  void* mem :pointer to data received buffer.
* @param  int len :received data length.
* @return int return received data length, length should equal to len given,
* length value <= 0 indicates connect error.
*/
static int blocking_lwip_recv(int socket, void* mem, int len)
{
    int readlen ,offset = 0, ret;
    fd_set fds;

// lwip dose not implement MSG_WAITALL, so we can't use block recv directly.    
//    return lwip_recv(socket, mem, len, MSG_WAITALL);
    while(1)
    {
        FD_ZERO(&fds);
        FD_SET(socket, &fds);
        if((ret = lwip_select(socket + 1, &fds, 0, 0, 0)) <= 0)
        {   /* select error. */
            printf("block receive select error.\n");
            return ret;
        }
        
        readlen = lwip_recv(socket, ((char*) mem) + offset, len - offset, 0);
        if(readlen <= 0)
        {   /* connect error, client has close the connection. */
            if(readlen < 0)
                printf("block receive error.\n");
            return readlen;
        }
        if(readlen == (len - offset))
        {   /* we have get all residuary data */
            offset = len;
            break;
        }
        offset += readlen;
    } // end of while(1)
    return offset;
}

/**
* @brief tcpserver accept tcp connect receive data and output on debug thermal.
* @param void* parameter :unused.
*/
static void tcpserv(void* parameter)
{
   char *recv_data; /* ���ڽ��յ�ָ�룬�������һ�ζ�̬��������������ڴ� */
   u32_t sin_size;
   int sock, connected, bytes_received, datalen;
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
   server_addr.sin_port = htons(TCPSERVER2_PORT_NO); /* ����˹����Ķ˿� */
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

   printf("\nTCPServer Waiting for client on port %d...\n", TCPSERVER2_PORT_NO);
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
           // �����Զ������ݰ���ͷ����
            bytes_received = blocking_lwip_recv(connected, recv_data, CUSTOM_HEADER_LEN);
            
            if(bytes_received != CUSTOM_HEADER_LEN)
                break;
            
            bytes_received = 0;
            // ����ͷ���ݻ�ȡʣ��������ݳ���
            if((datalen = CUSTOM_GET_LENGTH(recv_data)) > 0)
            {   // ����ʣ������
                bytes_received = blocking_lwip_recv(connected, recv_data + CUSTOM_HEADER_LEN, datalen);
                
                if(bytes_received != datalen) // ���յ������ݳ�����ʣ�����ݳ��Ȳ���
                    break;
            }
            
            // �������յ�������
            bytes_received = processCMD((unsigned char*)recv_data);
            
            if(bytes_received > 0) // ��Ҫ��ͻ��˷�������
                if(lwip_send(connected, recv_data, bytes_received, 0) < 0)
                    break;
            
       } // end of while(1)
       
       printf("close connection form (%s , %d).\n",
              inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
       lwip_close(connected); // �������ر�����
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
    OS_ERR err3;
    tcpserv(arg);
    
    printf("Delete Tcpserver thread.\n");
    OSTaskDel(&TcpServerTCB, &err3);
}

/**
* @brief start tcpserver
*/
void tcpserver2_init()
{
    // CPU_INT08U  os_err;
    
    // OSTaskCreate(tcpserver_thread, 
    //              NULL, 
    //              &TcpServerStack[TCPSERVER2_STK_SIZE - 1], 
    //              LWIP_TCPSERVER2_PRIO
    //                  );
    OS_ERR err3;

    OSTaskCreate((OS_TCB     *)&TcpServerTCB,
                (CPU_CHAR   *)"tcpserver2",
                (OS_TASK_PTR ) tcpserver_thread,
                (void       *) 0,
                (OS_PRIO     ) LWIP_TCPSERVER2_PRIO,
                (CPU_STK    *) &TcpServerStack[0],
                (CPU_STK_SIZE) TCPSERVER2_STK_SIZE / 10,
                (CPU_STK_SIZE) TCPSERVER2_STK_SIZE,
                (OS_MSG_QTY  ) 0,
                (OS_TICK     ) 0,
                (void       *) 0,
                (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                (OS_ERR     *)&err3);
}

/*-- File end --*/

