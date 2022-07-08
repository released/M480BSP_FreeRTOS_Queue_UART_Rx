# M480BSP_FreeRTOS_Queue_UART_Rx
 M480BSP_FreeRTOS_Queue_UART_Rx

update @ 2022/07/08

1. By using FreeRTOS with 3 task , and receive RX with PDMA function

	- vTask_check : to monitor Rx UART task

	- vTask_logger : regular logging task

	- vTask_uart : UART RX PDMA , chek define : ENABLE_PDMA_TIMEOUT , define :ENABLE_PDMA_SCATTER , for different UART RX PDMA behavior

	- PDMA Scatter-gather mode , refer from bleow link  

http://forum.nuvoton.com/viewtopic.php?f=19&t=10909&sid=15c7298de4911fcd1bb8cac333723b71

2. vTask_uart scenario :

	- task initial

		- create Queue for notice, to monitor PDMA trigger

		- create StreamBuffer , to receive UART Rx data under PDMA IRQ
		
		- IMPORTANT : NVIC_SetPriority , priority must bigger than configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY

	- task loop 
	
		- check Queue notice and prepare to get StreamBuffer data
		
		- also display PDMA received Rx data buffer , to compare between FreeRTOS StreamBuffer data

	- PDMA IRQ 
	
		- insert Queue for notice , and copy PDMA received Rx data into StreamBuffer 

3. below is log about ENABLE_PDMA_SCATTER

![image](https://github.com/released/M480BSP_FreeRTOS_Queue_UART_Rx/blob/main/uart_rx_pdma_scatter.jpg)	

below is log about ENABLE_PDMA_TIMEOUT

![image](https://github.com/released/M480BSP_FreeRTOS_Queue_UART_Rx/blob/main/uart_rx_pdma_timeout.jpg)	

below is teraterm setting  , to transmit UART data 

![image](https://github.com/released/M480BSP_FreeRTOS_Queue_UART_Rx/blob/main/tera_term_transmit.jpg)	


