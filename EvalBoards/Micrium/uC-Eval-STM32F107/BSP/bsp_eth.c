/**
  ******************************************************************************
  * @file    bsp_eth.c
  * @author  MCD Application Team
  * @version V1.0.2
  * @date    06-June-2011 
  * @brief   STM32F2x7 Ethernet hardware configuration.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <includes.h>
#include "bsp_eth.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define DP83848_PHY_ADDRESS       0x01 /* Relative to STM322xG-EVAL Board */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void ETH_GPIO_Config(void);
static void ETH_MACDMA_Config(void);

#define MII_MODE
/* Uncomment the define below to clock the PHY from external 25MHz crystal (only for MII mode) */
#ifdef 	MII_MODE
 #define PHY_CLOCK_MCO
#endif

#define ETH_TASK_STK_SIZE               TASK_STK_SIZE


/* Ethernet buffers */
extern ETH_DMADESCTypeDef  *DMATxDescToSet;
extern ETH_DMADESCTypeDef  *DMARxDescToGet;
extern ETH_DMADESCTypeDef  DMARxDscrTab[ETH_RXBUFNB]; 
extern ETH_DMADESCTypeDef  DMATxDscrTab[ETH_TXBUFNB];
extern unsigned char Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE], Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE];
extern ETH_DMA_Rx_Frame_infos *DMA_RX_FRAME_infos;
void  (*BSP_ETH_PostSem)(void) = NULL;
/* Private functions ---------------------------------------------------------*/

static void ETH_IRQHandler(void);



/**
  * @brief  Configures the Ethernet Interface
  * @param  None
  * @retval None
  */
static void ETH_MACDMA_Config(void)
{
  ETH_InitTypeDef ETH_InitStructure;

  /* Enable ETHERNET clock  */
  BSP_PeriphEn(BSP_PERIPH_ID_ETH);
  BSP_PeriphEn(BSP_PERIPH_ID_ETH_TX);
  BSP_PeriphEn(BSP_PERIPH_ID_ETH_RX);  
                                            
  /* Reset ETHERNET on AHB Bus */
  ETH_DeInit();

  /* Software reset */
  ETH_SoftwareReset();

  /* Wait for software reset */
  while (ETH_GetSoftwareResetStatus() == SET);

  /* ETHERNET Configuration --------------------------------------------------*/
  /* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
  ETH_StructInit(&ETH_InitStructure);

  /* Fill ETH_InitStructure parametrs */
  /*------------------------   MAC   -----------------------------------*/
  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
  //ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Disable; 
  //  ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
  //  ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;   

  ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
  ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
  ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
  ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
  ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
  ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
  ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
  ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
  ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

  /*------------------------   DMA   -----------------------------------*/  
  
  /* When we use the Checksum offload feature, we need to enable the Store and Forward mode: 
  the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum, 
  if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
  ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable; 
  ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;         
  ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;     
 
  ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;       
  ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;   
  ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
  ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;      
  ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;                
  ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;          
  ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
  ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

  /* Configure Ethernet */
  ETH_Init(&ETH_InitStructure, DP83848_PHY_ADDRESS);

  /* Enable the Ethernet Rx Interrupt */
  ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R, ENABLE);
}

/**
  * @brief  Configures the different GPIO ports.
  * @param  None
  * @retval None
  */
static void ETH_GPIO_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  /* Enable GPIOs clocks */
  BSP_PeriphEn(BSP_PERIPH_ID_IOPA);
  BSP_PeriphEn(BSP_PERIPH_ID_IOPB);
  BSP_PeriphEn(BSP_PERIPH_ID_IOPC);  
  BSP_PeriphEn(BSP_PERIPH_ID_IOPD);  


  /* Enable SYSCFG clock */
  BSP_PeriphEn(BSP_PERIPH_ID_SYSCFG); 
  /* Configure MCO (PA8) */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
  // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  // GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  // GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  /* MII/RMII Media interface selection --------------------------------------*/
#ifdef MII_MODE /* Mode MII with STM322xG-EVAL  */
 #ifdef PHY_CLOCK_MCO

  /* Output HSE clock (25MHz) on MCO pin (PA8) to clock the PHY */
  RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
 #endif /* PHY_CLOCK_MCO */

  SYSCFG_ETH_MediaInterfaceConfig(GPIO_ETH_MediaInterface_MII);
#elif defined RMII_MODE  /* Mode RMII with STM322xG-EVAL */

  /* Output PLL clock divided by 2 (50MHz) on MCO pin (PA8) to clock the PHY */
  RCC_MCO1Config(RCC_MCO1Source_PLLCLK, RCC_MCO1Div_2);

  SYSCFG_ETH_MediaInterfaceConfig(GPIO_ETH_MediaInterface_RMII);
#endif
  
/* Ethernet pins configuration ************************************************/
   /*
        ETH_MDIO                                   PA2
        ETH_MDC                                    PC1
        ETH_PPS_OUT                                PB5
        ETH_MII_CRS                                PA0
        ETH_MII_COL                                PA3
        ETH_MII_RX_ER                              PB10
        ETH_MII_RXD2                               PB0
        ETH_MII_RXD3                               PB1
        ETH_MII_TX_CLK                             PC3
        ETH_MII_TXD2                               PC2
        ETH_MII_TXD3                               PB8
        ETH_MII_RX_DV/ETH_RMII_CRS_DV              PA7
        ETH_MII_RX_CLK/ETH_RMII_REF_CLK            PA1
        ETH_MII_RXD0/ETH_RMII_RXD0                 PC4
        ETH_MII_RXD1/ETH_RMII_RXD1                 PC5
        ETH_MII_TX_EN/ETH_RMII_TX_EN               PB11
        ETH_MII_TXD0/ETH_RMII_TXD0                 PB12
        ETH_MII_TXD1/ETH_RMII_TXD1                 PB13

        ETH_RMII_CRS_DV                            PD8
        ETH_MII_RXD0/ETH_RMII_RXD0                 PD9
        ETH_MII_RXD1/ETH_RMII_RXD1                 PD10
    */

  /* Configure PA0, PA1, PA2, PA3 and PA7 */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_7;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);

  /* Configure PB0, PB1, PB10, PB11, PB12, PB13, PB5 and PB8 */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_5 | GPIO_Pin_8;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource1, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_ETH);	
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_ETH);

  /* Configure PC1, PC2, PC3, PC4 and PC5 */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource2, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource3, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);
}

/**
  * @brief  ETH_BSP_Config
  * @param  None
  * @retval None
  */
static void ETH_BSP_Config(void)
{
  /* Configure the GPIO ports for ethernet pins */
  ETH_GPIO_Config();
  /* Configure the Ethernet MAC/DMA */
  ETH_MACDMA_Config();
}
/**
  * @brief  OS Initializes the Ethernet
  * @param  void
  * @retval None
  */
void  BSP_InitEth(void)
{
  CPU_SR_ALLOC();  
  CPU_CRITICAL_ENTER();
  /* Configure the GPIO ports */
  ETH_BSP_Config();
  /* prefix vendor STMicroelectronics */
  ETH_MACAddressConfig(ETH_MAC_Address0,BSP_ETH_GetMacAddress());
 
  /* Initialize Tx Descriptors list: Chain Mode */
  ETH_DMATxDescChainInit(DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
  /* Initialize Rx Descriptors list: Chain Mode  */
  ETH_DMARxDescChainInit(DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);
 
  /* Rx interrupt enable */
  BSP_IntVectSet(BSP_INT_ID_ETH, ETH_IRQHandler);
  BSP_IntEn(BSP_INT_ID_ETH);    
  int i;
  for(i=0; i<ETH_RXBUFNB; ++i){
    ETH_DMARxDescReceiveITConfig(&DMARxDscrTab[i], ENABLE);
  }
  /* Enable the checksum insertion for the Tx frames */
  for(i=0; i<ETH_TXBUFNB; i++){
    ETH_DMATxDescChecksumInsertionConfig(&DMATxDscrTab[i], ETH_DMATxDesc_ChecksumTCPUDPICMPFull);
  }
  CPU_CRITICAL_EXIT();
  /* Enable MAC and DMA transmission and reception */
  ETH_Start();  
}
unsigned char * BSP_ETH_GetMacAddress(void)
{
      /* Enable CRC clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
    /* prefix vendor STMicroelectronics */
  static unsigned char Addr[] = {0, 0x80, 0xE1,0,0,0 };
  CRC_ResetDR();
  int i;
  u32 *w = (u32 *)0x1FFFF7E8,Crc32;
  for(i=0; i<3; ++i)
    Crc32 = CRC_CalcCRC(*w++);
  Crc32 = (0xFFFFFF&Crc32)+((0xFF000000&Crc32)>>24);
  Addr[3] = (Crc32 >> 16)&0xFF;
  Addr[4] = (Crc32 >> 8)&0xFF;
  Addr[5] = (Crc32)&0xFF;
  return Addr;
}




/*******************************************************************************
* Function Name  : ETH_GetCurrentTxBuffer
* Description    : Return the address of the buffer pointed by the current descritor.
* Input          : None
* Output         : None
* Return         : Buffer address
*******************************************************************************/
unsigned char * BSP_ETH_GetCurrentTxBuffer(void)
{
  unsigned char * tx_addr = (unsigned char*)DMATxDescToSet->Buffer1Addr;
  /* Return Buffer address */
  return tx_addr;   
}


uint32_t BSP_ETH_IsRxPktValid(void)
{
  return ETH_GetRxPktSize(); 
}


/**
  * @brief  OS ETH_IRQHandler()
  *
  * IT function to receive and send Ethernet packet
  * @param  void
  * @retval None
  */
  /* Handles all the received frames */
static void ETH_IRQHandler(void)
{
  /* Frame received */
  if ( ETH_GetDMAFlagStatus(ETH_DMA_FLAG_R) == SET){
    /* Give the semaphore to wakeup LwIP task */
    if(BSP_ETH_PostSem)
      BSP_ETH_PostSem();
  }
	
  /* Clear the interrupt flags. */
  /* Clear the Eth DMA Rx IT pending bits */
  ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
  ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
  OSIntExit();
}

void BSP_ETH_ReleaseDescriptor(FrameTypeDef * pframe)
{
  int i;
   __IO ETH_DMADESCTypeDef *DMARxNextDesc;
  /* Release descriptors to DMA */
  /* Check if received frame with multiple DMA buffer segments */
  if (DMA_RX_FRAME_infos->Seg_Count > 1){
    DMARxNextDesc = DMA_RX_FRAME_infos->FS_Rx_Desc;
  }else{
    DMARxNextDesc = pframe->descriptor;
  }
  
  /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
  for (i=0; i<DMA_RX_FRAME_infos->Seg_Count; i++){  
    DMARxNextDesc->Status = ETH_DMARxDesc_OWN;
    DMARxNextDesc = (ETH_DMADESCTypeDef *)(DMARxNextDesc->Buffer2NextDescAddr);
  }
  
  /* Clear Segment_Count */
  DMA_RX_FRAME_infos->Seg_Count =0;
  
  
  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((ETH->DMASR & ETH_DMASR_RBUS) != (u32)RESET){
    /* Clear RBUS ETHERNET DMA flag */
    ETH->DMASR = ETH_DMASR_RBUS;
      
    /* Resume DMA reception */
    ETH->DMARPDR = 0;
  }
}
bool BSP_ETH_GetDescriptorStatus(FrameTypeDef * pframe)
{
  return ((pframe->descriptor->Status & ETH_DMARxDesc_ES) == (uint32_t)RESET)?
    1:0;
}
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
