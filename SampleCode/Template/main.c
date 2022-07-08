/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "Queue.h"
#include "timers.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "cpu_utils.h"


#include "project_config.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

// #define printf(...)	\
// {	\
// 	vTaskSuspendAll();	\
// 	printf(__VA_ARGS__);	\
// 	fflush(stdout); \
// 	xTaskResumeAll();	\
// }	\

#define mainNORMAL_TASK_PRIORITY           	( tskIDLE_PRIORITY + 0UL )
#define mainABOVENORMAL_TASK_PRIORITY     	( mainNORMAL_TASK_PRIORITY + 1UL )

#define mainCHECK_TASK_PRIORITY             ( mainNORMAL_TASK_PRIORITY + 3UL )

QueueHandle_t xUARTQueue_timeout; 
QueueHandle_t xUARTQueue_scatter; 
StreamBufferHandle_t xStreamBuffer_timeout;
StreamBufferHandle_t xStreamBuffer_scatter;

unsigned int sRxCount = 0;


/*_____ D E F I N I T I O N S ______________________________________________*/
volatile uint32_t BitFlag = 0;
volatile uint32_t counter_tick = 0;

#define UART1_RX_DMA_CH 		            (0)
#define UART2_RX_DMA_CH 		            (1)
#define UART_PDMA_OPENED_CH   	            ((1 << UART1_RX_DMA_CH) | (1 << UART2_RX_DMA_CH))

#define UART1_PDMA_OPENED_CH_RX   	        (1 << UART1_RX_DMA_CH)
#define UART2_PDMA_OPENED_CH_RX   	        (1 << UART2_RX_DMA_CH)

#define PDMA_TIME                           (0x100)
#define RXBUFSIZE                           (32)

#define PDMA_GET_TRANS_CNT(pdma,u32Ch)      ((uint32_t)((pdma->DSCT[(u32Ch)].CTL&PDMA_DSCT_CTL_TXCNT_Msk) >> PDMA_DSCT_CTL_TXCNT_Pos))

#define ENABLE_UART1
// #define ENABLE_UART2

// #define ENABLE_PDMA_TIMEOUT
#define ENABLE_PDMA_SCATTER

#if defined (ENABLE_PDMA_TIMEOUT)
#define PDMA_TIMEOUT_IRQHandler            PDMA_IRQHandler

uint8_t UART1_RxBuffer[RXBUFSIZE] = {0};
uint8_t UART2_RxBuffer[RXBUFSIZE] = {0};

#elif defined (ENABLE_PDMA_SCATTER)
#define PDMA_SCATTER_IRQHandler            PDMA_IRQHandler

enum
{
    UART1_RX_BUFFER01 = 0,
    UART1_RX_BUFFER02,

    UART2_RX_BUFFER01 = 0,
    UART2_RX_BUFFER02,    
};

typedef struct DMA_DESC_t
{
    uint32_t ctl;
    uint32_t src;
    uint32_t dest;
    uint32_t offset;
} DMA_DESC_t;

DMA_DESC_t DMA_UART1DESC[2];
DMA_DESC_t DMA_UART2DESC[2];

typedef struct UART_packet_T
{
    uint8_t g_u8RecData[RXBUFSIZE];
    uint32_t g_u32comRbytes;
    uint32_t g_u32comRhead;
    uint32_t g_u32comRtail;
    uint32_t g_u32cntbak;    
    uint32_t g_u8PackageComplete;
    uint16_t tmp; 
    uint32_t cnt;     
} UART_packet_T;
UART_packet_T UART1_P =  
{
    {0} ,   //g_u8RecData[RXBUFSIZE]
    0,
    0,
    0,
    RXBUFSIZE / 2 - 1,  //g_u32cntbak
    0,
    0,
    0
};

UART_packet_T UART2_P =  
{
    {0} ,   //g_u8RecData[RXBUFSIZE]
    0,
    0,
    0,
    RXBUFSIZE / 2 - 1,  //g_u32cntbak
    0,
    0,
    0
};

#endif


/*_____ M A C R O S ________________________________________________________*/
#define DELAY_MS					        (1)

/*_____ F U N C T I O N S __________________________________________________*/

void tick_counter(void)
{
	counter_tick++;
}

uint32_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void compare_buffer(uint8_t *src, uint8_t *des, int nBytes)
{
    uint16_t i = 0;	
	
    for (i = 0; i < nBytes; i++)
    {
        if (src[i] != des[i])
        {
            printf("error idx : %4d : 0x%2X , 0x%2X\r\n", i , src[i],des[i]);
			set_flag(flag_error , ENABLE);
        }
    }

	if (!is_flag_set(flag_error))
	{
    	printf("%s finish \r\n" , __FUNCTION__);	
		set_flag(flag_error , DISABLE);
	}

}

void reset_buffer(void *dest, unsigned int val, unsigned int size)
{
    uint8_t *pu8Dest;
//    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;

	#if 1
	while (size-- > 0)
		*pu8Dest++ = val;
	#else
	memset(pu8Dest, val, size * (sizeof(pu8Dest[0]) ));
	#endif
	
}

void copy_buffer(void *dest, void *src, unsigned int size)
{
    uint8_t *pu8Src, *pu8Dest;
    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;
    pu8Src  = (uint8_t *)src;


	#if 0
	  while (size--)
	    *pu8Dest++ = *pu8Src++;
	#else
    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
	#endif
}

void dump_buffer(uint8_t *pucBuff, int nBytes)
{
    uint16_t i = 0;
    
    printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        printf("0x%2X," , pucBuff[i]);
        if ((i+1)%8 ==0)
        {
            printf("\r\n");
        }            
    }
    printf("\r\n\r\n");
}

void dump_buffer_hex(uint8_t *pucBuff, int nBytes)
{
    int     nIdx, i;

    nIdx = 0;
    while (nBytes > 0)
    {
        printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            printf("%02X ", pucBuff[nIdx + i]);
        printf("  ");
        for (i = 0; i < 16; i++)
        {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
            {
                printf("%c", pucBuff[nIdx + i]);
            }
            else
            {
                printf(".");
            }
            nBytes--;
        }
        nIdx += 16;
        printf("\n");
    }
    printf("\n");
}

void StreamBufferInISR(StreamBufferHandle_t xStreamBuffer, uint8_t* RxIn ,size_t xDataLengthBytes)
{
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;    
    BaseType_t xResult;    

    xResult = xStreamBufferSendFromISR( xStreamBuffer, RxIn , xDataLengthBytes, &xHigherPriorityTaskWoken );

    /* Was the message posted successfully? */
    if( xResult != pdFAIL ) {
        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
        switch should be requested.  The macro used is port specific and will
        be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
        the documentation page for the port being used. */
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

void QueueSingleInISR(QueueHandle_t xQueue, uint8_t tag)
{
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;    
    BaseType_t xResult;    
    uint8_t xData = tag;

    xResult = xQueueSendFromISR( xQueue, &xData, &xHigherPriorityTaskWoken );   
    printf("Queue ISR:0x%2X ,tag:0x%2X\r\n" , xResult , xData);        

    /* Was the message posted successfully? */
    if( xResult != pdFAIL ) {
    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
    switch should be requested.  The macro used is port specific and will
    be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
    the documentation page for the port being used. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

#if defined (ENABLE_PDMA_SCATTER)

void PDMA_SCATTER_IRQHandler(void)
{
    uint32_t status = PDMA_GET_INT_STATUS(PDMA);
    uint8_t len = 0;

    #if defined (ENABLE_UART1) 
    UART1_P.cnt = PDMA_GET_TRANS_CNT(PDMA, UART1_RX_DMA_CH); /* channel transfer count */
    #endif

    #if defined (ENABLE_UART2) 
    UART2_P.cnt = PDMA_GET_TRANS_CNT(PDMA, UART2_RX_DMA_CH); /* channel transfer count */
    #endif

    if (status & PDMA_INTSTS_ABTIF_Msk)   /* abort */
    {
        printf("target abort interrupt !!:\r\n");
		#if 0
        PDMA_CLR_ABORT_FLAG(PDMA, PDMA_GET_ABORT_STS(PDMA));
		#else

        #if defined (ENABLE_UART1)            
        if (PDMA_GET_ABORT_STS(PDMA) & (UART1_PDMA_OPENED_CH_RX))
        {
            printf("UART1_PDMA_OPENED_CH_RX\r\n");
            PDMA_CLR_ABORT_FLAG(PDMA, (UART1_PDMA_OPENED_CH_RX));
        }
        #endif
        
        #if defined (ENABLE_UART2) 
        if (PDMA_GET_ABORT_STS(PDMA) & (UART2_PDMA_OPENED_CH_RX))
        {
            printf("UART2_PDMA_OPENED_CH_RX\r\n");
            PDMA_CLR_ABORT_FLAG(PDMA, (UART2_PDMA_OPENED_CH_RX));
        }
        #endif
        
		#endif
    }
    else if (status & PDMA_INTSTS_TDIF_Msk)     /* done */
    {
        #if defined (ENABLE_UART1)            
        if (PDMA_GET_TD_STS(PDMA) & UART1_PDMA_OPENED_CH_RX)
        {
            PDMA_CLR_TD_FLAG(PDMA, UART1_PDMA_OPENED_CH_RX);

            /* Update receive count */
            UART1_P.g_u32comRbytes += UART1_P.g_u32cntbak + 1; /* count */
            UART1_P.g_u32comRtail = (UART1_P.g_u32comRtail + UART1_P.g_u32cntbak + 1) % RXBUFSIZE; /* index */
            UART1_P.g_u32cntbak = RXBUFSIZE / 2 - 1; /* channel cnt backup */

            UART1_P.g_u8PackageComplete = 1;

            QueueSingleInISR(xUARTQueue_scatter,0x55);  
        } 
        #endif

        #if defined (ENABLE_UART2) 
        if (PDMA_GET_TD_STS(PDMA) & UART2_PDMA_OPENED_CH_RX)
        {
            PDMA_CLR_TD_FLAG(PDMA, UART2_PDMA_OPENED_CH_RX);

            /* Update receive count */
            UART2_P.g_u32comRbytes += UART2_P.g_u32cntbak + 1; /* count */
            UART2_P.g_u32comRtail = (UART2_P.g_u32comRtail + UART2_P.g_u32cntbak + 1) % RXBUFSIZE; /* index */
            UART2_P.g_u32cntbak = RXBUFSIZE / 2 - 1; /* channel cnt backup */

            UART2_P.g_u8PackageComplete = 1;
        } 
        #endif

    }
    #if defined (ENABLE_UART1)     
    else if (status & (PDMA_INTSTS_REQTOF0_Msk))     /* Check the DMA time-out interrupt flag */
    {
        // printf("UART1_RX time-out !!\r\n");
        /* Update receive count */
        UART1_P.g_u32comRbytes += UART1_P.g_u32cntbak - UART1_P.cnt;  /* count */
        UART1_P.g_u32comRtail = (UART1_P.g_u32comRtail + UART1_P.g_u32cntbak - UART1_P.cnt) % RXBUFSIZE;  /* index */
        UART1_P.g_u32cntbak = UART1_P.cnt; /* channel cnt backup */

        UART1_P.g_u8PackageComplete = 0;

        #if 0 // debug
        printf("2)Rhead:%2d\r\n",UART1_P.g_u32comRhead); 
        printf("2)Rtail:%2d\r\n",UART1_P.g_u32comRtail);         
        printf("2)Rbytes:%2d\r\n",UART1_P.g_u32comRbytes); 
        #endif
        if ( (UART1_P.g_u32comRbytes + UART1_P.g_u32comRhead) > (RXBUFSIZE) )
        {
            if (( (UART1_P.g_u32comRbytes + UART1_P.g_u32comRhead) - RXBUFSIZE  ) == UART1_P.g_u32comRtail)
            {
                len = UART1_P.g_u32comRbytes - UART1_P.g_u32comRtail;
                StreamBufferInISR(xStreamBuffer_scatter, &UART1_P.g_u8RecData[UART1_P.g_u32comRhead], len);
                #if 0 // debug
                printf("2a)len : %2d\r\n",len);
                #endif

                len = UART1_P.g_u32comRtail;
                StreamBufferInISR(xStreamBuffer_scatter, &UART1_P.g_u8RecData[0], len);
                #if 0 // debug
                printf("2b)len : %2d\r\n",len);
                #endif
            }            
        }
        else
        {
            len = UART1_P.g_u32comRbytes + 0;            
            StreamBufferInISR(xStreamBuffer_scatter, &UART1_P.g_u8RecData[UART1_P.g_u32comRhead], len);
            #if 0 // debug
            printf("2c)len : %2d\r\n",len);
            #endif           
        }
        
        QueueSingleInISR(xUARTQueue_scatter,0x66);        

        /* restart timeout */
        PDMA_SetTimeOut(PDMA, UART1_RX_DMA_CH, DISABLE, 0);
        PDMA_CLR_TMOUT_FLAG(PDMA, UART1_RX_DMA_CH);
        PDMA_SetTimeOut(PDMA, UART1_RX_DMA_CH, ENABLE, PDMA_TIME);
        sRxCount++; 
    }
    #endif
    #if defined (ENABLE_UART2)     
    else if (status & (PDMA_INTSTS_REQTOF1_Msk))     /* Check the DMA time-out interrupt flag */
    {
        // printf("UART2_RX time-out !!\r\n");
        /* Update receive count */
        UART2_P.g_u32comRbytes += UART2_P.g_u32cntbak - UART2_P.cnt;  /* count */
        UART2_P.g_u32comRtail = (UART2_P.g_u32comRtail + UART2_P.g_u32cntbak - UART2_P.cnt) % RXBUFSIZE;  /* index */
        UART2_P.g_u32cntbak = UART2_P.cnt; /* channel cnt backup */

        UART2_P.g_u8PackageComplete = 0;

        /* restart timeout */
        PDMA_SetTimeOut(PDMA, UART2_RX_DMA_CH, DISABLE, 0);
        PDMA_CLR_TMOUT_FLAG(PDMA, UART2_RX_DMA_CH);
        PDMA_SetTimeOut(PDMA, UART2_RX_DMA_CH, ENABLE, PDMA_TIME);        
    }    
    #endif
    else
    {
        printf("unknown interrupt !!\r\n");
    }	
}


void PDMA_WriteUART2SGTable(void)  //scatter-gather table */
{
    DMA_UART2DESC[UART2_RX_BUFFER01].ctl = \
                      ((RXBUFSIZE / 2 - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | /* Transfer count is RXBUFSIZE/2 */ \
                      PDMA_WIDTH_8 |    /* Transfer width is 8 bits */ \
                      PDMA_SAR_FIX |    /* Source increment size is  fixed(no increment) */ \
                      PDMA_DAR_INC |    /* Destination increment size is 8 bits */ \
                      PDMA_REQ_SINGLE | /* Transfer type is single transfer type */ \
                      PDMA_BURST_1 |    /* Burst size is 1. No effect in single transfer type */ \
                      PDMA_OP_SCATTER;  /* Operation mode is scatter-gather mode */
    /* Configure source address */
    DMA_UART2DESC[UART2_RX_BUFFER01].src = (uint32_t)UART2_BASE;
    /* Configure destination address */
    DMA_UART2DESC[UART2_RX_BUFFER01].dest = (uint32_t)&UART2_P.g_u8RecData[0];/*buffer 1 */
    /* Configure next descriptor table address */
    DMA_UART2DESC[UART2_RX_BUFFER01].offset = (uint32_t)&DMA_UART2DESC[UART2_RX_BUFFER02] - (PDMA->SCATBA); /* next operation table is table 2 */

    DMA_UART2DESC[UART2_RX_BUFFER02].ctl = \
                      ((RXBUFSIZE / 2 - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | /* Transfer count is RXBUFSIZE/2 */ \
                      PDMA_WIDTH_8 |    /* Transfer width is 8 bits */ \
                      PDMA_SAR_FIX |    /* Source increment size is fixed(no increment) */ \
                      PDMA_DAR_INC |    /* Destination increment size is 8 bits */ \
                      PDMA_REQ_SINGLE | /* Transfer type is single transfer type */ \
                      PDMA_BURST_1 |    /* Burst size is 1. No effect in single transfer type */ \
                      PDMA_OP_SCATTER;  /* Operation mode is scatter-gather mode */
    /* Configure source address */
    DMA_UART2DESC[UART2_RX_BUFFER02].src = (uint32_t)UART2_BASE;
    /* Configure destination address */
    DMA_UART2DESC[UART2_RX_BUFFER02].dest = (uint32_t)&UART2_P.g_u8RecData[RXBUFSIZE / 2] ; /* buffer 2 */
    /* Configure next descriptor table address */
    DMA_UART2DESC[UART2_RX_BUFFER02].offset = (uint32_t)&DMA_UART2DESC[UART2_RX_BUFFER01] - (PDMA->SCATBA); /* next operation table is table 1 */

}

void PDMA_WriteUART1SGTable(void)  //scatter-gather table */
{
    DMA_UART1DESC[UART1_RX_BUFFER01].ctl = \
                      ((RXBUFSIZE / 2 - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | /* Transfer count is RXBUFSIZE/2 */ \
                      PDMA_WIDTH_8 |    /* Transfer width is 8 bits */ \
                      PDMA_SAR_FIX |    /* Source increment size is  fixed(no increment) */ \
                      PDMA_DAR_INC |    /* Destination increment size is 8 bits */ \
                      PDMA_REQ_SINGLE | /* Transfer type is single transfer type */ \
                      PDMA_BURST_1 |    /* Burst size is 1. No effect in single transfer type */ \
                      PDMA_OP_SCATTER;  /* Operation mode is scatter-gather mode */
    /* Configure source address */
    DMA_UART1DESC[UART1_RX_BUFFER01].src = (uint32_t)UART1_BASE;
    /* Configure destination address */
    DMA_UART1DESC[UART1_RX_BUFFER01].dest = (uint32_t)&UART1_P.g_u8RecData[0];/*buffer 1 */
    /* Configure next descriptor table address */
    DMA_UART1DESC[UART1_RX_BUFFER01].offset = (uint32_t)&DMA_UART1DESC[UART1_RX_BUFFER02] - (PDMA->SCATBA); /* next operation table is table 2 */

    DMA_UART1DESC[UART1_RX_BUFFER02].ctl = \
                      ((RXBUFSIZE / 2 - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | /* Transfer count is RXBUFSIZE/2 */ \
                      PDMA_WIDTH_8 |    /* Transfer width is 8 bits */ \
                      PDMA_SAR_FIX |    /* Source increment size is fixed(no increment) */ \
                      PDMA_DAR_INC |    /* Destination increment size is 8 bits */ \
                      PDMA_REQ_SINGLE | /* Transfer type is single transfer type */ \
                      PDMA_BURST_1 |    /* Burst size is 1. No effect in single transfer type */ \
                      PDMA_OP_SCATTER;  /* Operation mode is scatter-gather mode */
    /* Configure source address */
    DMA_UART1DESC[UART1_RX_BUFFER02].src = (uint32_t)UART1_BASE;
    /* Configure destination address */
    DMA_UART1DESC[UART1_RX_BUFFER02].dest = (uint32_t)&UART1_P.g_u8RecData[RXBUFSIZE / 2] ; /* buffer 2 */
    /* Configure next descriptor table address */
    DMA_UART1DESC[UART1_RX_BUFFER02].offset = (uint32_t)&DMA_UART1DESC[UART1_RX_BUFFER01] - (PDMA->SCATBA); /* next operation table is table 1 */
}

void UART_PDMA_SCATTER_Init(void)
{

    SYS_ResetModule(PDMA_RST);
    
    #if defined (ENABLE_UART1)    
    PDMA_Open(PDMA, UART1_PDMA_OPENED_CH_RX);    
    PDMA_WriteUART1SGTable();
    PDMA_SetTransferMode(PDMA, UART1_RX_DMA_CH, PDMA_UART1_RX, TRUE, (uint32_t)&DMA_UART1DESC[UART1_RX_BUFFER01]);
    PDMA_SetTimeOut(PDMA,UART1_RX_DMA_CH, ENABLE, PDMA_TIME);
    PDMA_EnableInt(PDMA, UART1_RX_DMA_CH, PDMA_INT_TRANS_DONE);
    PDMA_EnableInt(PDMA, UART1_RX_DMA_CH, PDMA_INT_TIMEOUT);   
    #endif

    #if defined (ENABLE_UART2)
    PDMA_Open(PDMA, UART2_PDMA_OPENED_CH_RX);    
    PDMA_WriteUART2SGTable();
    PDMA_SetTransferMode(PDMA, UART2_RX_DMA_CH, PDMA_UART2_RX, TRUE, (uint32_t)&DMA_UART2DESC[UART2_RX_BUFFER01]);
    PDMA_SetTimeOut(PDMA,UART2_RX_DMA_CH, ENABLE, PDMA_TIME);    
    PDMA_EnableInt(PDMA, UART2_RX_DMA_CH, PDMA_INT_TRANS_DONE);
    PDMA_EnableInt(PDMA, UART2_RX_DMA_CH, PDMA_INT_TIMEOUT);
    #endif

    //
    // Note:
    //  If you will call xxxFromISR API from ISR, the peripheral's interrupt priority
    //  must bigger than 'configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY', or the API will
    //  block by the FreeRTOS kernel.
    //
    NVIC_SetPriority(PDMA_IRQn, 0x06);
    NVIC_EnableIRQ(PDMA_IRQn);
}
#endif

#if defined (ENABLE_PDMA_TIMEOUT)

void usart_rx_check(void) 
{
    static size_t old_pos;
    size_t pos;

    /* Calculate current position in buffer and check for new data available */
    pos = SIZEOF(UART1_RxBuffer) - PDMA_GET_TRANS_CNT(PDMA, UART1_RX_DMA_CH);
    if (pos != old_pos) {                       /* Check change in received data */
        if (pos > old_pos) {                    /* Current position is over previous one */
            /*
             * Processing is done in "linear" mode.
             *
             * Application processing is fast with single data block,
             * length is simply calculated by subtracting pointers
             *
             * [   0   ]
             * [   1   ] <- old_pos |------------------------------------|
             * [   2   ]            |                                    |
             * [   3   ]            | Single block (len = pos - old_pos) |
             * [   4   ]            |                                    |
             * [   5   ]            |------------------------------------|
             * [   6   ] <- pos
             * [   7   ]
             * [ N - 1 ]
             */
            dump_buffer(&UART1_RxBuffer[old_pos], pos - old_pos);
        } else {
            /*
             * Processing is done in "overflow" mode..
             *
             * Application must process data twice,
             * since there are 2 linear memory blocks to handle
             *
             * [   0   ]            |---------------------------------|
             * [   1   ]            | Second block (len = pos)        |
             * [   2   ]            |---------------------------------|
             * [   3   ] <- pos
             * [   4   ] <- old_pos |---------------------------------|
             * [   5   ]            |                                 |
             * [   6   ]            | First block (len = N - old_pos) |
             * [   7   ]            |                                 |
             * [ N - 1 ]            |---------------------------------|
             */
            dump_buffer(&UART1_RxBuffer[old_pos], SIZEOF(UART1_RxBuffer) - old_pos);
            if (pos > 0) {
                dump_buffer(&UART1_RxBuffer[0], pos);
            }
        }
        old_pos = pos;                          /* Save current position as old for next transfers */
    }
}

void UART2_RX_PDMA_set(void)
{
    //RX	
    PDMA_SetTransferCnt(PDMA,UART2_RX_DMA_CH, PDMA_WIDTH_8, RXBUFSIZE);
    PDMA_SetTransferAddr(PDMA,UART2_RX_DMA_CH, UART2_BASE, PDMA_SAR_FIX, ((uint32_t) (&UART2_RxBuffer[0])), PDMA_DAR_INC);	
    /* Set request source; set basic mode. */
    PDMA_SetTransferMode(PDMA,UART2_RX_DMA_CH, PDMA_UART2_RX, FALSE, 0);
      
}

void UART1_RX_PDMA_set(void)
{
    //RX	
    PDMA_SetTransferCnt(PDMA,UART1_RX_DMA_CH, PDMA_WIDTH_8, RXBUFSIZE);
    PDMA_SetTransferAddr(PDMA,UART1_RX_DMA_CH, UART1_BASE, PDMA_SAR_FIX, ((uint32_t) (&UART1_RxBuffer[0])), PDMA_DAR_INC);	
    /* Set request source; set basic mode. */
    PDMA_SetTransferMode(PDMA,UART1_RX_DMA_CH, PDMA_UART1_RX, FALSE, 0);
     
}

void PDMA_TIMEOUT_IRQHandler(void)
{
    uint32_t status = PDMA_GET_INT_STATUS(PDMA);    

    if (status & PDMA_INTSTS_ABTIF_Msk)   /* abort */
    {
        printf("target abort interrupt !!:\r\n");
		#if 0
        PDMA_CLR_ABORT_FLAG(PDMA, PDMA_GET_ABORT_STS(PDMA));
		#else

        #if defined (ENABLE_UART1)            
        if (PDMA_GET_ABORT_STS(PDMA) & (UART1_PDMA_OPENED_CH_RX))
        {
            printf("UART1_PDMA_OPENED_CH_RX\r\n");
            PDMA_CLR_ABORT_FLAG(PDMA, (UART1_PDMA_OPENED_CH_RX));
        }
        #endif
        
        #if defined (ENABLE_UART2) 
        if (PDMA_GET_ABORT_STS(PDMA) & (UART2_PDMA_OPENED_CH_RX))
        {
            printf("UART2_PDMA_OPENED_CH_RX\r\n");
            PDMA_CLR_ABORT_FLAG(PDMA, (UART2_PDMA_OPENED_CH_RX));
        }
        #endif
        
		#endif
    }
    else if (status & PDMA_INTSTS_TDIF_Msk)     /* done */
    {
		#if 1

        #if defined (ENABLE_UART1)            
        if (PDMA_GET_TD_STS(PDMA) & UART1_PDMA_OPENED_CH_RX)
        {
            PDMA_CLR_TD_FLAG(PDMA, UART1_PDMA_OPENED_CH_RX);
        } 
        #endif

        #if defined (ENABLE_UART2) 
        if (PDMA_GET_TD_STS(PDMA) & UART2_PDMA_OPENED_CH_RX)
        {
            PDMA_CLR_TD_FLAG(PDMA, UART2_PDMA_OPENED_CH_RX);
        } 
        #endif
		
		#else
        if((PDMA_GET_TD_STS(PDMA) & UART_PDMA_OPENED_CH) == UART_PDMA_OPENED_CH)
        {
            /* Clear PDMA transfer done interrupt flag */
            PDMA_CLR_TD_FLAG(PDMA, UART_PDMA_OPENED_CH);
			//insert process
			/*
                DISABLE TRIGGER
            */

        } 
		#endif
    }
    #if defined (ENABLE_UART1)     
    else if (status & (PDMA_INTSTS_REQTOF0_Msk))     /* Check the DMA time-out interrupt flag */
    {
        // printf("UART1_RX time-out !!\r\n");
        /* Update receive count */

        StreamBufferInISR(xStreamBuffer_timeout,UART1_RxBuffer,RXBUFSIZE);
        QueueSingleInISR(xUARTQueue_timeout,0xA5);
    
        // PDMA_SET_TRANS_CNT(PDMA, UART1_RX_DMA_CH,1); 
        /* restart timeout */
        PDMA_SetTimeOut(PDMA, UART1_RX_DMA_CH, DISABLE, 0);
        PDMA_CLR_TMOUT_FLAG(PDMA, UART1_RX_DMA_CH);
        PDMA_SetTimeOut(PDMA, UART1_RX_DMA_CH, ENABLE, PDMA_TIME);    
        set_flag(flag_UART1_RX_end,ENABLE);
        sRxCount++;       
    }
    #endif
    #if defined (ENABLE_UART2)     
    else if (status & (PDMA_INTSTS_REQTOF1_Msk))     /* Check the DMA time-out interrupt flag */
    {
        // printf("UART2_RX time-out !!\r\n");
        /* Update receive count */
        
        // PDMA_SET_TRANS_CNT(PDMA, UART2_RX_DMA_CH,1);    
        /* restart timeout */
        PDMA_SetTimeOut(PDMA, UART2_RX_DMA_CH, DISABLE, 0);
        PDMA_CLR_TMOUT_FLAG(PDMA, UART2_RX_DMA_CH);
        PDMA_SetTimeOut(PDMA, UART2_RX_DMA_CH, ENABLE, PDMA_TIME);    
        set_flag(flag_UART2_RX_end,ENABLE);          
    }    
    #endif    
    else
    {
        printf("unknown interrupt !!\r\n");
    }	

    // portEND_SWITCHING_ISR( xHigherPriorityTaskWoken ); 

}

void UART_PDMA_TIMEOUT_Init(void)
{

	set_flag(flag_UART1_RX_end,DISABLE);
	set_flag(flag_UART2_RX_end,DISABLE);

    SYS_ResetModule(PDMA_RST);

    #if defined (ENABLE_UART1)    
    PDMA_Open(PDMA, UART1_PDMA_OPENED_CH_RX);

    PDMA_SetTransferCnt(PDMA,UART1_RX_DMA_CH, PDMA_WIDTH_8, RXBUFSIZE);
    /* Set source/destination address and attributes */
    PDMA_SetTransferAddr(PDMA,UART1_RX_DMA_CH, UART1_BASE, PDMA_SAR_FIX, ((uint32_t) (&UART1_RxBuffer[0])), PDMA_DAR_INC);

    /* Set request source; set basic mode. */
    PDMA_SetTransferMode(PDMA,UART1_RX_DMA_CH, PDMA_UART1_RX, FALSE, 0);
    /* Single request type. */
    PDMA_SetBurstType(PDMA,UART1_RX_DMA_CH, PDMA_REQ_SINGLE, 0);
    /* Disable table interrupt */
    PDMA_DisableInt(PDMA,UART1_RX_DMA_CH, PDMA_INT_TEMPTY );//PDMA->DSCT[UART1_RX_DMA_CH].CTL |= PDMA_DSCT_CTL_TBINTDIS_Msk;  

    PDMA_EnableInt(PDMA, UART1_RX_DMA_CH, PDMA_INT_TRANS_DONE);
    PDMA_EnableInt(PDMA, UART1_RX_DMA_CH, PDMA_INT_TIMEOUT);

    // PDMA->TOUTPSC = (PDMA->TOUTPSC & (~PDMA_TOUTPSC_TOUTPSC0_Msk)) | (0x5 << PDMA_TOUTPSC_TOUTPSC0_Pos);
    PDMA_SetTimeOut(PDMA,UART1_RX_DMA_CH, ENABLE, PDMA_TIME );

    #endif

    #if defined (ENABLE_UART2)     
    PDMA_Open(PDMA, UART2_PDMA_OPENED_CH_RX);

    PDMA_SetTransferCnt(PDMA,UART2_RX_DMA_CH, PDMA_WIDTH_8, RXBUFSIZE);
    /* Set source/destination address and attributes */
    PDMA_SetTransferAddr(PDMA,UART2_RX_DMA_CH, UART2_BASE, PDMA_SAR_FIX, ((uint32_t) (&UART2_RxBuffer[0])), PDMA_DAR_INC);
    /* Set request source; set basic mode. */
    PDMA_SetTransferMode(PDMA,UART2_RX_DMA_CH, PDMA_UART2_RX, FALSE, 0);
    /* Single request type. */
    PDMA_SetBurstType(PDMA,UART2_RX_DMA_CH, PDMA_REQ_SINGLE, 0);
    /* Disable table interrupt */
    PDMA_DisableInt(PDMA,UART2_RX_DMA_CH, PDMA_INT_TEMPTY );//PDMA->DSCT[UART2_RX_DMA_CH].CTL |= PDMA_DSCT_CTL_TBINTDIS_Msk;  

    PDMA_EnableInt(PDMA, UART2_RX_DMA_CH, PDMA_INT_TRANS_DONE);
    PDMA_EnableInt(PDMA, UART2_RX_DMA_CH, PDMA_INT_TIMEOUT); 

    // PDMA->TOUTPSC = (PDMA->TOUTPSC & (~PDMA_TOUTPSC_TOUTPSC1_Msk)) | (0x5 << PDMA_TOUTPSC_TOUTPSC1_Pos);
    PDMA_SetTimeOut(PDMA,UART2_RX_DMA_CH, ENABLE, PDMA_TIME );     
    #endif
    
     /*
        UART data freq : 1.6KHz	0.625	ms

        Set PDMA CH 0/1 timeout to about 
        2 ms (5/(72M/(2^15)))
        0.56 ms (5/(72M/(2^13)))

        target (ms)	    u32TimeOutCnt	clk div	    prescale	
        2.275555556	    5	            72000000	32768	15      111
        1.137777778	    5	            72000000	16384	14      110
        0.568888889	    5	            72000000	8192	13      101
        0.284444444	    5	            72000000	4096	12      100
        0.142222222	    5	            72000000	2048	11      011
        0.071111111	    5	            72000000	1024	10      010
        0.035555556	    5	            72000000	512	    9       001
        0.017777778	    5	            72000000	256	    8       000  

    */

    //
    // Note:
    //  If you will call xxxFromISR API from ISR, the peripheral's interrupt priority
    //  must bigger than 'configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY', or the API will
    //  block by the FreeRTOS kernel.
    //
    NVIC_SetPriority(PDMA_IRQn, 0x06);
    NVIC_EnableIRQ(PDMA_IRQn);
}
#endif

/*
    UART1 RX :PA2
    UART2 RX :PB0
*/
void UART1_UART2_Init(void)
{
    #if defined (ENABLE_UART1)     
    SYS_ResetModule(UART1_RST);
    UART_Open(UART1, 115200);
    UART_PDMA_ENABLE(UART1,UART_INTEN_RXPDMAEN_Msk);    
    #endif

    #if defined (ENABLE_UART2)     
    SYS_ResetModule(UART2_RST);    
    UART_Open(UART2, 115200);
    UART_PDMA_ENABLE(UART2,UART_INTEN_RXPDMAEN_Msk);    
    #endif

}


void TMR1_IRQHandler(void)
{
	// static uint32_t LOG = 0;

	
    if(TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
		tick_counter();

		if ((get_tick() % 1000) == 0)
		{
			// printf("%s : %4d\r\n",__FUNCTION__,LOG++);
			PH0 ^= 1;
		}

		if ((get_tick() % 50) == 0)
		{

		}	
    }
}


void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
    TIMER_Start(TIMER1);
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(UART0);

	if (res > 0x7F)
	{
		printf("invalid command\r\n");
	}
	else
	{
		switch(res)
		{
			case '1':
				break;

			case 'X':
			case 'x':
			case 'Z':
			case 'z':
				NVIC_SystemReset();		
				break;
		}
	}
}


void UART0_IRQHandler(void)
{
    if(UART_GET_INT_FLAG(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))     /* UART receive data available flag */
    {
        while(UART_GET_RX_EMPTY(UART0) == 0)
        {
			UARTx_Process();
        }
    }

    if(UART0->FIFOSTS & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART_ClearIntFlag(UART0, (UART_INTSTS_RLSINT_Msk| UART_INTSTS_BUFERRINT_Msk));
    }	
}

void UART0_Init(void)
{
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);

	/* Set UART receive time-out */
	UART_SetTimeoutCnt(UART0, 20);

	UART0->FIFO &= ~UART_FIFO_RFITL_4BYTES;
	UART0->FIFO |= UART_FIFO_RFITL_8BYTES;

	/* Enable UART Interrupt - */
	UART_ENABLE_INT(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_TOCNTEN_Msk | UART_INTEN_RXTOIEN_Msk);
	
	NVIC_EnableIRQ(UART0_IRQn);

	#if (_debug_log_UART_ == 1)	//debug
	printf("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetCPUFreq());
	printf("CLK_GetHXTFreq : %8d\r\n",CLK_GetHXTFreq());
	printf("CLK_GetLXTFreq : %8d\r\n",CLK_GetLXTFreq());	
	printf("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetPCLK0Freq());
	printf("CLK_GetPCLK1Freq : %8d\r\n",CLK_GetPCLK1Freq());	

//    printf("Product ID 0x%8X\n", SYS->PDID);

    printf("\r\n\r\n");
	
	#endif
}

void Custom_Init(void)
{
	SYS->GPH_MFPL = (SYS->GPH_MFPL & ~(SYS_GPH_MFPL_PH0MFP_Msk)) | (SYS_GPH_MFPL_PH0MFP_GPIO);
	SYS->GPH_MFPL = (SYS->GPH_MFPL & ~(SYS_GPH_MFPL_PH1MFP_Msk)) | (SYS_GPH_MFPL_PH1MFP_GPIO);
	SYS->GPH_MFPL = (SYS->GPH_MFPL & ~(SYS_GPH_MFPL_PH2MFP_Msk)) | (SYS_GPH_MFPL_PH2MFP_GPIO);

	//EVM LED
	GPIO_SetMode(PH,BIT0,GPIO_MODE_OUTPUT);
	GPIO_SetMode(PH,BIT1,GPIO_MODE_OUTPUT);
	GPIO_SetMode(PH,BIT2,GPIO_MODE_OUTPUT);
	
}

void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Set XT1_OUT(PF.2) and XT1_IN(PF.3) to input mode */
    PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);

    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_LXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);

    /* Set core clock as PLL_CLOCK from PLL */
    CLK_SetCoreClock(FREQ_192MHZ);
    /* Set PCLK0/PCLK1 to HCLK/2 */
    CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);

    CLK_EnableModuleClock(PDMA_MODULE);

    /* Enable UART clock */
    CLK_EnableModuleClock(UART0_MODULE);
    /* Select UART clock source from HXT */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));

    #if defined (ENABLE_UART1) 
    CLK_EnableModuleClock(UART1_MODULE);
    /* Select UART clock source from HXT */
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART1SEL_HIRC, CLK_CLKDIV0_UART1(1));
    #endif

    #if defined (ENABLE_UART2) 
    CLK_EnableModuleClock(UART2_MODULE);
    /* Select UART clock source from HXT */
    CLK_SetModuleClock(UART2_MODULE, CLK_CLKSEL3_UART2SEL_HIRC, CLK_CLKDIV4_UART2(1));
    #endif

    /* Set GPB multi-function pins for UART0 RXD and TXD */
    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk);
    SYS->GPB_MFPH |= (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

    #if defined (ENABLE_UART1) 
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA2MFP_Msk);
    SYS->GPA_MFPL |= SYS_GPA_MFPL_PA2MFP_UART1_RXD;
    #endif

    #if defined (ENABLE_UART2) 
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB0MFP_Msk);
    SYS->GPB_MFPL |= SYS_GPB_MFPL_PB0MFP_UART2_RXD;    
    #endif

    CLK_EnableModuleClock(TMR1_MODULE);
    CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HIRC, 0);
	
    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}



void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created.  It is also called by various parts of the
    demo application.  If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}
/*-----------------------------------------------------------*/

#if 0   // under cpu_utils.c
void vApplicationIdleHook( void )
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
    to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
    task.  It is essential that code added to this hook function never attempts
    to block in any way (for example, call xQueueReceive() with a block time
    specified, or call vTaskDelay()).  If the application makes use of the
    vTaskDelete() API function (as this demo application does) then it is also
    important that vApplicationIdleHook() is permitted to return to its calling
    function, because it is the responsibility of the idle task to clean up
    memory allocated by the kernel to any task that has since been deleted. */
}
#endif

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}
/*-----------------------------------------------------------*/

#if 0   // under cpu_utils.c
void vApplicationTickHook( void )
{
    /* This function will be called by each tick interrupt if
    configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
    added here, but the tick hook is called from an interrupt context, so
    code must not attempt to block, and only the interrupt safe FreeRTOS API
    functions can be used (those that end in FromISR()).  */

#if ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 0 )
    {
        /* In this case the tick hook is used as part of the queue set test. */
        vQueueSetAccessQueueSetFromISR();
    }
#endif /* mainCREATE_SIMPLE_BLINKY_DEMO_ONLY */
}
#endif
/*-----------------------------------------------------------*/


portBASE_TYPE xAreTaskUartStillRunning( void )
{
    static short sLastRxCount = 0;
    portBASE_TYPE xReturn;

    /* Not too worried about mutual exclusion on these variables as they are 16
     * bits and we are only reading them.  We also only care to see if they have
     * changed or not. */

    if ( sRxCount == sLastRxCount ) 
    {
        xReturn = pdFALSE;
    }
    else
    {
        xReturn = pdTRUE;
    }

    sLastRxCount = sRxCount;

    return xReturn;
}

void vTask_uart( void *pvParameters )
{	 
    uint8_t xData = 0;
    uint8_t xDataStreamBuffer[RXBUFSIZE] = {0};

    #if defined (ENABLE_PDMA_TIMEOUT)
	xUARTQueue_timeout = xQueueCreate( 10 , sizeof( unsigned char ) );
    xStreamBuffer_timeout = xStreamBufferCreate( RXBUFSIZE, RXBUFSIZE/2 );

    UART_PDMA_TIMEOUT_Init();

    #elif defined (ENABLE_PDMA_SCATTER)
	xUARTQueue_scatter = xQueueCreate( RXBUFSIZE , sizeof( unsigned char ) );
    xStreamBuffer_scatter = xStreamBufferCreate( RXBUFSIZE, RXBUFSIZE/2 );

    UART_PDMA_SCATTER_Init();
    #endif

    UART1_UART2_Init();

    printf("%s is running ...\r\n",__FUNCTION__);

	(void) pvParameters;
	for( ;; )
	{ 
        PH2 ^= 1;

        #if defined (ENABLE_PDMA_TIMEOUT)    
        if ( xQueueReceive(xUARTQueue_timeout, &xData, ( ( TickType_t ) 1 / portTICK_PERIOD_MS ) ))   
        {
            printf("\r\nread Queue : 0x%2X\r\n", xData);
            if ( xStreamBufferReceive(xStreamBuffer_timeout, &xDataStreamBuffer, RXBUFSIZE,( ( TickType_t ) 1 / portTICK_PERIOD_MS ) ))   
            {
                printf("StreamBuffer ready(timeout)\r\n");
            } 

            if (is_flag_set(flag_UART1_RX_end))
            {
                set_flag(flag_UART1_RX_end, DISABLE);

                printf("UART1_RxBuffer : \r\n");
                // dump_buffer(UART1_RxBuffer, RXBUFSIZE);
                dump_buffer_hex(UART1_RxBuffer, RXBUFSIZE); 
                reset_buffer(UART1_RxBuffer,0x00,RXBUFSIZE); 
                
                printf("xDataStreamBuffer : \r\n");
                // dump_buffer(xDataStreamBuffer, RXBUFSIZE);
                dump_buffer_hex(xDataStreamBuffer, RXBUFSIZE); 
                reset_buffer(xDataStreamBuffer,0x00,RXBUFSIZE);  
                           
                UART1_RX_PDMA_set();        
            }

            if (is_flag_set(flag_UART2_RX_end))
            {
                set_flag(flag_UART2_RX_end, DISABLE);

                printf("UART2_RxBuffer : \r\n");
                dump_buffer(UART2_RxBuffer, RXBUFSIZE);
                reset_buffer(UART2_RxBuffer,0x00,RXBUFSIZE);  

                UART2_RX_PDMA_set();   
            }
        }

        #endif

        #if defined (ENABLE_PDMA_SCATTER)
        if ( xQueueReceive(xUARTQueue_scatter, &xData, ( ( TickType_t ) 5 / portTICK_PERIOD_MS ) ))   
        {
            printf("\r\nread Queue : 0x%2X\r\n",xData);

            if (xData == 0x66)
            {
                if ( xStreamBufferReceive(xStreamBuffer_scatter, &xDataStreamBuffer, RXBUFSIZE,( ( TickType_t ) 1 / portTICK_PERIOD_MS ) ))   
                {
                    printf("StreamBuffer ready(scatter)\r\n");
                    
                    printf("xDataStreamBuffer : \r\n");
                    // dump_buffer(xDataStreamBuffer, RXBUFSIZE);
                    dump_buffer_hex(xDataStreamBuffer, RXBUFSIZE); 
                    reset_buffer(xDataStreamBuffer,0x00,RXBUFSIZE);                              
                }    
            }         
        }

        #if defined (ENABLE_UART1)     
        if (UART1_P.g_u32comRbytes)
        {        
            UART1_P.tmp = UART1_P.g_u32comRtail;

            if (UART1_P.g_u32comRhead != UART1_P.tmp)
            {
                printf("UART1:%c[0x%2X](Rbytes:%2d,Rhead:%2d,Rtail:%2d)\r\n", 
                UART1_P.g_u8RecData[UART1_P.g_u32comRhead],
                UART1_P.g_u8RecData[UART1_P.g_u32comRhead],            
                UART1_P.g_u32comRbytes,
                UART1_P.g_u32comRhead,
                UART1_P.g_u32comRtail);

                UART1_P.g_u32comRhead = (UART1_P.g_u32comRhead == (RXBUFSIZE - 1)) ? 0 : (UART1_P.g_u32comRhead + 1);
                UART1_P.g_u32comRbytes--;
            }
        }
        #endif

        #if defined (ENABLE_UART2) 
        if (UART2_P.g_u32comRbytes)
        {        
            UART2_P.tmp = UART2_P.g_u32comRtail;

            if (UART2_P.g_u32comRhead != UART2_P.tmp)
            {
                printf("UART2:%c[0x%2X](Rbytes:%2d,Rhead:%2d,Rtail:%2d)\r\n", 
                UART2_P.g_u8RecData[UART2_P.g_u32comRhead],
                UART2_P.g_u8RecData[UART2_P.g_u32comRhead],            
                UART2_P.g_u32comRbytes,
                UART2_P.g_u32comRhead,
                UART2_P.g_u32comRtail);            
                
                UART2_P.g_u32comRhead = (UART2_P.g_u32comRhead == (RXBUFSIZE - 1)) ? 0 : (UART2_P.g_u32comRhead + 1);
                UART2_P.g_u32comRbytes--;
            }
        }
        #endif   

        #endif

        /* Add data processing code */

	}  
}

void vTask_logger( void *pvParameters )
{	
 	// static uint32_t cnt = 0;   
	uint32_t millisec = DELAY_MS*1000;
	portTickType xLastWakeTime;

	xLastWakeTime = xTaskGetTickCount();

    printf("%s is running ...\r\n",__FUNCTION__);

	(void) pvParameters;
	for( ;; )
	{
        vTaskDelayUntil( &xLastWakeTime, ( ( portTickType ) millisec / portTICK_RATE_MS));
        
        #if 0 // debug
        printf("Task logger :%4d (heap:%3dbytes ,CPU:%3d)\r\n" ,cnt++,xPortGetFreeHeapSize(),osGetCPUUsage());
        #endif
		
        PH1 ^= 1;
	}  
}

void vTask_check( void *pvParameters )
{	
    static uint32_t timecount = 0;
	uint32_t millisec = DELAY_MS*5000;
    uint32_t res = 0;

	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

    timecount = get_tick();

    printf("%s is running ...\r\n",__FUNCTION__);

	(void) pvParameters;
	for( ;; )
	{
        vTaskDelayUntil( &xLastWakeTime, ( ( portTickType ) millisec / portTICK_RATE_MS));
        res = get_tick();
        if( xAreTaskUartStillRunning() != pdTRUE )
        {
            #if 0 // debug
            printf( "ERROR IN TaskUart (prev:%5d,current:%5d,diff:%5d)\r\n" , timecount , res , res-timecount);
            timecount = get_tick();
            #endif            
        }
	}  
}



/*
 * This is a template project for M480 series MCU. Users could based on this project to create their
 * own application without worry about the IAR/Keil project settings.
 *
 * This template application uses external crystal as HCLK source and configures UART0 to print out
 * "Hello World", users may need to do extra system configuration based on their system design.
 */

int main()
{
    SYS_Init();

	UART0_Init();
	Custom_Init();	
	TIMER1_Init();

    xTaskCreate(vTask_check     ,"check"    ,configMINIMAL_STACK_SIZE   ,NULL   ,mainCHECK_TASK_PRIORITY        ,NULL);
    xTaskCreate(vTask_logger    ,"logger"   ,configMINIMAL_STACK_SIZE   ,NULL	,mainNORMAL_TASK_PRIORITY       ,NULL);
    xTaskCreate(vTask_uart      ,"uart"     ,configMINIMAL_STACK_SIZE*2 ,NULL	,mainNORMAL_TASK_PRIORITY       ,NULL);

    vTaskStartScheduler();

    for( ;; );
}

/*** (C) COPYRIGHT 2016 Nuvoton Technology Corp. ***/
