/**
* @file lwIP.c
* @brief A brief file description.
* @details
*     A more elaborated file description.
* @author Wang Mengyin
* @date 2010Jul19 15:53:34
* @note
*               Copyright 2010 Wang Mengyin.ALL RIGHTS RESERVED.
*                            http://tigerwang202.blogbus.com
*    This software is provided under license and contains proprietary and
* confidential material which is the property of Company Name tech.
*/


#define __LW_IP_C


/* Includes ------------------------------------------------------------------*/
#include "memp.h"
#include "lwIP.h"
#include "tcp.h"
#include "udp.h"
#include "tcpip.h"
#include "etharp.h"
#include "dhcp.h"
#include "ethernetif.h"
// #include "stm32f10x.h"
#include "arch/sys_arch.h"
#include <stdio.h>
// #include "stm3210c_eval_lcd.h"
#include "stm32_eth.h"

static OS_SEM InitLwipSem;

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define MAX_DHCP_TRIES        4
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static struct netif netif;
static uint32_t IPaddress = 0;

/* Private function prototypes -----------------------------------------------*/
static void TcpipInitDone(void *arg);
static void list_if(void);

/* Private functions ---------------------------------------------------------*/

/** 
 * @brief TcpipInitDone wait for tcpip init being done
 * 
 * @param arg the semaphore to be signaled
 */
static void TcpipInitDone(void *arg)
{
    // sys_sem_t *sem;
    // sem = arg;
    sys_sem_signal((OS_SEM*)arg);
}

/**
* @brief Init_lwIP initialize the LwIP
*/
void Init_lwIP(void)
{
    struct ip_addr ipaddr;
    struct ip_addr netmask;
    struct ip_addr gw;
    uint8_t macaddress[6]={0xAA,0xAA,0xAA,0xAA,0xAA,0xAA};

    OS_ERR err3;
    
    // sys_sem_t sem;
    
    // sys_init();
    
    /* Initializes the dynamic memory heap defined by MEM_SIZE.*/
    // mem_init();
    
    /* Initializes the memory pools defined by MEMP_NUM_x.*/
    // memp_init();
    
    // pbuf_init();	
    // netif_init();
    
    // sem = sys_sem_new(0);
    tcpip_init(TcpipInitDone, &InitLwipSem);
    // sys_sem_wait(sem);
    OSSemPend(&InitLwipSem, 0, OS_OPT_PEND_BLOCKING, NULL, &err3);
    // sys_sem_free(sem);
    OSSemDel(&InitLwipSem, OS_OPT_DEL_ALWAYS, &err3);
    
    // rx_sem = sys_sem_new(0);                // create receive semaphore
    // tx_sem = sys_sem_new(0);                // create transmit semaphore
    
    // sys_thread_new("Rx Thread", Rx_Thread, &netif, LWIP_STK_SIZE, DrvRx_Thread_PRIO); //lwIP Task 1
    // sys_thread_new(Tx_Thread, NULL, DrvTx_Thread_PRIO); //lwIP Task 2
    // and lwIP Task 3 was used for TCPIP thread
      
#if LWIP_DHCP
    /* ����DHCP������ */
    ipaddr.addr = 0;
    netmask.addr = 0;
    gw.addr = 0;
#else
    /* ���þ�̬IP */
    IP4_ADDR(&ipaddr, 192, 168, 0, 10);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 0, 1);
#endif
    
    Set_MAC_Address(macaddress);
    
    netif_add(&netif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);
    netif_set_default(&netif);
#if LWIP_DHCP
    dhcp_start(&netif);
#endif
    netif_set_up(&netif);
}

/** 
 * @brief Display_IPAddress Display IP Address
 * 
 */
void Display_IPAddress(void)
{
    if(IPaddress != netif.ip_addr.addr)
    {   /* IP ��ַ�����ı�*/
        __IO uint8_t iptab[4];
        uint8_t iptxt[20];
        
        /* read the new IP address */
        IPaddress = netif.ip_addr.addr;
        
        iptab[0] = (uint8_t)(IPaddress >> 24);
        iptab[1] = (uint8_t)(IPaddress >> 16);
        iptab[2] = (uint8_t)(IPaddress >> 8);
        iptab[3] = (uint8_t)(IPaddress);
        
        sprintf((char*)iptxt, "   %d.%d.%d.%d    ", iptab[3], iptab[2], iptab[1], iptab[0]);
        
        list_if();
        
        /* Display the new IP address */
#if LWIP_DHCP
        if(netif.flags & NETIF_FLAG_DHCP)
        {   // IP��ַ��DHCPָ��
            // Display the IP address
            printf("IP assigned\nby DHCP server %s\n", iptxt);
        }
        else
#endif  // ��̬IP��ַ
        {   /* Display the IP address */
            printf("Static IP Addr %s\n", iptxt);
//            LCD_Puts("0123456789abcdef" "123456789abcdef0" "23456789abcdef01" "3456789abcdef012");
        }
        
    }
#if LWIP_DHCP
    else if(IPaddress == 0)
    {   // �ȴ�DHCP����IP
        printf("Looking for DHCP server please wait ...\n");
        
        /* If no response from a DHCP server for MAX_DHCP_TRIES times */
        /* stop the dhcp client and set a static IP address */
        if(netif.dhcp->tries > MAX_DHCP_TRIES) 
        {   /* ����DHCP���Դ��������þ�̬IP���� */
            struct ip_addr ipaddr;
            struct ip_addr netmask;
            struct ip_addr gw;
            
            printf("DHCP timeout\n");
            dhcp_stop(&netif);
            
            IP4_ADDR(&ipaddr, 10, 21, 11, 245);
            IP4_ADDR(&netmask, 255, 255, 255, 0);
            IP4_ADDR(&gw, 10, 21, 11, 254);
            
            netif_set_addr(&netif, &ipaddr , &netmask, &gw);
            
            list_if();
        }
    }
#endif
}

/**
* @brief display ip address in serial port debug windows
*/
static void list_if()
{
    printf("Default network interface: %c%c\n", netif.name[0], netif.name[1]);
    printf("ip address: %s\n", inet_ntoa(*((struct in_addr*)&(netif.ip_addr))));
    printf("gw address: %s\n", inet_ntoa(*((struct in_addr*)&(netif.gw))));
    printf("net mask  : %s\n", inet_ntoa(*((struct in_addr*)&(netif.netmask))));
    
}

/*---------Ethernte ISR Added by Wang 2010-07-20------------------------------*/
/**
  * @brief  Ethernet ISR
  * @param  None
  * @retval None
  */
void LwIP_Pkt_Handle(void)
{
    
    while(ETH_GetRxPktSize() != 0)
    {
        ethernetif_input(&netif);
    }
    
    /* Clear Rx Pending Bit */
    ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
    /* Clear the Eth DMA Rx IT pending bits */
    ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
    
}
/*-- File end --*/

