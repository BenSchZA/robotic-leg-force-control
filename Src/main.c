/**
 ******************************************************************************
 * File Name          : main.c
 * Description        : Main program body
 ******************************************************************************
 *
 * COPYRIGHT(c) 2016 STMicroelectronics
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */

/*******************************************************************************
* Author: Benjamin Scholtz
* Contact: bscholtz.bds@gmail.com
* Purpose: Mechatronic Engineering Undergrad Thesis: Baleka - Robotic Hopping Leg
* Tools: STM32CubeMX, FreeRTOS, HAL
*******************************************************************************/

#include <string.h>
#include <stdio.h>

#include "CRC.h"
#include "arrayFunctions.h"

//https://github.com/PetteriAimonen/Baselibc
//#include "memccpy.c"
//#include "memcmp.c"


/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart6_rx;

osThreadId defaultTaskHandle;
osThreadId TXPCHandle;
osThreadId RXPCHandle;
osThreadId HeartbeatHandle;
osThreadId TXMotor1Handle;
osThreadId TXMotor2Handle;
osThreadId RXMotor1Handle;
osThreadId RXMotor2Handle;
osMessageQId ProcessQM1Handle;
osMessageQId ProcessQM2Handle;
osMessageQId TransmitQHandle;
osMessageQId ProcessQiNemoHandle;
osMessageQId ProcessQPCHandle;
osMessageQId TransmitM1QHandle;
osMessageQId TransmitM2QHandle;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
uint8_t Ts = 5; //Sampling time in ms

//PC Buffer

//Motor Buffer

uint8_t RXBufM1[20];
uint8_t RXBufM2[20];

/* WHAT ARE ALL THE USARTS AND TIMERS USED FOR AND WHICH PINS DO THEY USE?
 *
 * USART1 = COMMS between the PC and STM	---		PB6 (TX)	 PB7 (RX)
 * USART2 = COMMS to Motor Controller 1		---		PA2 (TX)	 PA3 (RX)
 * USART3 = COMMS to Motor Controller 2		---		PC10 (TX)    PC11 (RX)
 * USART6 = COMMS to iNEMO					---		PC6 (TX)	 PC7 (RX)
 *
 * TIM1 = Timer for Center Boom Encoder		---		PE9 (A)	     PE11 (B)
 * TIM2 = Timer for Pitch Encoder			---		PA15 (A)	 PB3 (B)
 * TIM3 = Used for 5ms interrupts
 * TIM4 = Timer for Linear Encoder			---		PD12 (A)     PD13 (B)
 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void const * argument);
void StartTXPC(void const * argument);
void StartRXPC(void const * argument);
void StartHeartbeat(void const * argument);
void StartTXMotor1(void const * argument);
void StartTXMotor2(void const * argument);
void StartRXMotor1(void const * argument);
void StartRXMotor2(void const * argument);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

//Select Call-backs functions called after Transfer complete
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

//Packet Protocol ############################################################
//'packet' makes sure compiler won't insert any gaps!

//To PC

//Mutex to protect shared resource
SemaphoreHandle_t xSemaphoreTX = NULL;

struct __attribute__((__packed__)) TXPacketStruct {
        uint8_t START[2];

        uint8_t LENGTH;

        uint8_t M1C[2];
        uint8_t M1P[4];
        uint8_t M1V[4];

        uint8_t M2C[2];
        uint8_t M2P[4];
        uint8_t M2V[4];

        uint8_t ACCX[2];
        uint8_t ACCY[2];
        uint8_t ACCZ[2];
        uint8_t GYRX[2];
        uint8_t GYRY[2];
        uint8_t GYRZ[2];
        uint8_t TEMP;
        uint8_t StatBIT_1 : 1;
        uint8_t StatBIT_2 : 1;
        uint8_t StatBIT_3 : 1;
        uint8_t StatBIT_4 : 1;
        uint8_t StatBIT_5 : 1;
        uint8_t StatBIT_6 : 1;
        uint8_t StatBIT_7 : 1;
        uint8_t StatBIT_8 : 1;

        uint8_t CRCCheck;

        uint8_t STOP[2];
};

struct TXPacketStruct PCPacket;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *PCPacketPTR = (uint8_t*)&PCPacket;



//Example Usage

//PCPacket.ACCX[0] = 0xAA;
//PCPacket.STOP[0] = 0xAA;
//PCPacket.STOP[1] = 0xAA;
//PCPacket.StatBIT_2 = 1;
//PCPacket.StatBIT_8 = 1;
//sizeof(PCPacket);
//PCPacketPTR[n];

//From PC
struct __attribute__((__packed__)) RXPacketStruct {
        uint8_t START[2];

        uint8_t LENGTH;

        uint8_t M1C[4];
        uint8_t M2C[4];

        uint8_t StatBIT_1 : 1;
        uint8_t StatBIT_2 : 1;
        uint8_t StatBIT_3 : 1;
        uint8_t StatBIT_4 : 1;
        uint8_t StatBIT_5 : 1;
        uint8_t StatBIT_6 : 1;
        uint8_t StatBIT_7 : 1;
        uint8_t StatBIT_8 : 1;

        uint8_t CRCCheck;

        uint8_t STOP[2];
};

struct RXPacketStruct RXPacket;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *RXPacketPTR = (uint8_t*)&RXPacket;

//Heartbeat ##################################################################
#define TASK_TXM1        ( 1 << 0 )
#define TASK_TXM2        ( 1 << 1 )
#define TASK_RXM1        ( 1 << 2 )
#define TASK_RXM2        ( 1 << 3 )
#define TASK_HEARTBEAT   ( 1 << 4 )

////Message IDs
#define ID_KILL 0
#define ID_WRITE 1
#define ID_BRIDGE 2
#define ID_CURRENT_SET 3
#define ID_CURRENT_DATA 4
#define ID_POSITION_DATA 5
#define ID_VELOCITY_DATA 6

#define ALL_SYNC_BITS ( TASK_HEARTBEAT | TASK_TXM1 | TASK_TXM2 | TASK_RXM1 | TASK_RXM2 )

/* Declare a variable to hold the created event group. */
EventGroupHandle_t xEventSyncDriver;

//Motor Packets
//'packed' makes sure compiler won't insert any gaps!
struct __attribute__((__packed__)) HeartPacketStruct {
        uint8_t KILL[14];
        uint8_t WRITE[14];
        uint8_t BRIDGE[14];
        uint8_t CURRENT_SET[14];
        uint8_t CURRENT_DATA[14];
        uint8_t POSITION_DATA[14];
        uint8_t VELOCITY_DATA[14];
};

//Same size packets allows access
struct HeartPacketStruct MotorPacket;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *MotorPacketPTR = (uint8_t*)&MotorPacket;

// Command Byte (byte 3 of packet):
//
// TX->Reply
// Kill Bridge
// 00xx xx10 -> 00xx xx00
// 0000 0110 -> 0000 0100
// 0x06 -> 0x04
// 0xCBB6
// Write Enable
// 00xx xx10 -> 00xx xx00
// 0000 1010 -> 0000 1000
// 0x0A -> 0x08
// 0x3624
// Bridge Enable
// 00xx xx10 -> 00xx xx00
// 0001 0010 -> 0001 0000
// 0x12 -> 0x10
// 0x1AE0
// Set Current (0)
// 00xx xx10 -> 00xx xx00
// 0000 1110 -> 0000 1100
// 0x0E -> 0x0C
// 0xBF7B
// Read Current (2)
// 00xx xx01 -> 00xx xx10
// 0011 0001 -> 0011 0010
// 0x31 -> 0x32
// 0x9772
// Read Velocity (4)
// 00xx xx01 -> 00xx xx10
// 0011 1101 -> 0011 1110
// 0x3D -> 0x3E
// 0xD310
// Read Position (4)
// 00xx(2) xx(1)01 -> 00xx xx10
// 0001 0101 -> 0001 0110
// 0x15 -> 0x16
// 0x5EAF
// Opcode: xx xx
// CHANGE CRC!!!!!!!!!!!!!!!!!!!!!!!!!

uint8_t KILL[12] = {0xA5, 0x3F, 0x06, 0x01, 0x00, 0x01, 0xCB, 0xB6, 0x01, 0x00, 0x33, 0x31};
uint8_t WRITE[12] = {0xA5, 0x3F, 0x0A, 0x07, 0x00, 0x01, 0x36, 0x24, 0x0F, 0x00, 0x10, 0x3E};
uint8_t BRIDGE[12] = {0xA5, 0x3F, 0x12, 0x01, 0x00, 0x01, 0x1A, 0xE0, 0x00, 0x00, 0x00, 0x00};
uint8_t CURRENT_SET[14] = {0xA5, 0x3F, 0x0E, 0x45, 0x00, 0x02, 0xBF, 0x7B, 0x48 /*Current[3]*/, 0x01 /*Current[2]*/, 0x00 /*Current[1]*/, 0x00 /*Current[0]*/, 0xDC, 0x6F}; //TODO Set Current 0
uint8_t CURRENT_DATA[8] = {0xA5, 0x3F, 0x31, 0x10, 0x03, 0x01, 0x97, 0x72};
uint8_t POSITION_DATA[8] = {0xA5, 0x3F, 0x3D, 0x12, 0x00, 0x02, 0x5E, 0xAF};
uint8_t VELOCITY_DATA[8] = {0xA5, 0x3F, 0x15, 0x11, 0x02, 0x02, 0xD3, 0x10};

uint8_t *MotorPacketArray[7] = {KILL,WRITE,BRIDGE,CURRENT_SET,CURRENT_DATA,POSITION_DATA,VELOCITY_DATA};

struct __attribute__((__packed__)) MotorPacketSize {
        uint8_t KILL;
        uint8_t WRITE;
        uint8_t BRIDGE;
        uint8_t CURRENT_SET;
        uint8_t CURRENT_DATA;
        uint8_t POSITION_DATA;
        uint8_t VELOCITY_DATA;
};

struct MotorPacketSize MotorPacketSize;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *MotorPacketSizePTR = (uint8_t*)&MotorPacketSize;

uint8_t KILL_L = 12;
uint8_t WRITE_L = 12;
uint8_t BRIDGE_L = 12;
uint8_t CURRENT_SET_L = 14;
uint8_t CURRENT_DATA_L = 8;
uint8_t POSITION_DATA_L = 8;
uint8_t VELOCITY_DATA_L = 8;

//Motor Replies
// struct HeartStruct {
//         struct KILL {
//                 uint8_t TX_HEADER[6];
//                 uint8_t TX_CRC1[2];
//                 uint8_t TX_DATA[2];
//                 uint8_t TX_CRC2[2];
//                 uint8_t RX_HEADER[6];
//                 uint8_t RX_CRC1[2];
//         } KILL;
//         struct WRITE {
//                 uint8_t TX_HEADER[6];
//                 uint8_t TX_CRC1[2];
//                 uint8_t TX_DATA[2];
//                 uint8_t TX_CRC2[2];
//                 uint8_t RX_HEADER[6];
//                 uint8_t RX_CRC1[2];
//         } WRITE;
//         struct BRIDGE {
//                 uint8_t TX_HEADER[6];
//                 uint8_t TX_CRC1[2];
//                 uint8_t TX_DATA[2];
//                 uint8_t TX_CRC2[2];
//                 uint8_t RX_HEADER[6];
//                 uint8_t RX_CRC1[2];
//         } BRIDGE;
//         struct CURRENT_SET {
//                 uint8_t TX_HEADER[6];
//                 uint8_t TX_CRC1[2];
//                 uint8_t TX_DATA[4];
//                 uint8_t TX_CRC2[2];
//                 uint8_t RX_HEADER[6];
//                 uint8_t RX_CRC1[2];
//         } CURRENT_SET;
//         struct CURRENT_DATA {
//                 uint8_t TX_HEADER[6];
//                 uint8_t TX_CRC1[2];
//                 uint8_t RX_HEADER[6];
//                 uint8_t RX_CRC1[2];
//                 uint8_t RX_DATA[2];
//                 uint8_t RX_CRC2[2];
//         } CURRENT_DATA;
//         struct POSITION_DATA {
//                 uint8_t TX_HEADER[6];
//                 uint8_t TX_CRC1[2];
//                 uint8_t RX_HEADER[6];
//                 uint8_t RX_CRC1[2];
//                 uint8_t RX_DATA[4];
//                 uint8_t RX_CRC2[2];
//         } POSITION_DATA;
//         struct VELOCITY_DATA {
//                 uint8_t TX_HEADER[6];
//                 uint8_t TX_CRC1[2];
//                 uint8_t RX_HEADER[6];
//                 uint8_t RX_CRC1[2];
//                 uint8_t RX_DATA[4];
//                 uint8_t RX_CRC2[2];
//         } VELOCITY_DATA;
// };

//struct HeartStruct MotorHeart;
////Transmit pointer PCPacketPTR with sizeof(PCPacket)
//uint8_t *MotorHeartPTR = (uint8_t*)&MotorHeart;
//memccpy(MotorHeart.KILL.TX_HEADER, test, 0, 2);

union {
        uint32_t WORD;
        uint16_t HALFWORD;
        uint8_t BYTE[4];
} WORDtoBYTE;

//Message from the "heart"
struct HeartMessage
{
        uint8_t ucMessageID;
        uint8_t *ucData;
};

struct HeartMessage xHeartM1;
struct HeartMessage xHeartM2;


//#############################################################################

//Motor functions
void SetupMotors(void);
void SetupArrays(void);

void ClearBuf(uint8_t buf[], uint8_t size); //Set buffer contents to 0
void SetBuf(uint8_t buf[], uint8_t set[], uint8_t size); //Set buf[] to set[]
void SetBytes(uint8_t buf[], uint8_t pos1, uint8_t val1, uint8_t pos2, uint8_t val2, uint8_t pos3, uint8_t val3, uint8_t pos4, uint8_t val4); //For setting current commands, set pos to NULL to omit
void TransmitM1_DMA(uint8_t *data, uint8_t size);
void ReceiveM1_DMA(uint8_t *data, uint8_t size);
void TransmitM2_DMA(uint8_t *data, uint8_t size);
void ReceiveM2_DMA(uint8_t *data, uint8_t size);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

        /* USER CODE BEGIN 1 */

        /* USER CODE END 1 */

        /* MCU Configuration----------------------------------------------------------*/

        /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
        HAL_Init();

        /* Configure the system clock */
        SystemClock_Config();

        /* Initialize all configured peripherals */
        MX_GPIO_Init();
        MX_DMA_Init();
        MX_USART2_UART_Init();
        MX_USART3_UART_Init();
        MX_USART6_UART_Init();
        MX_USART1_UART_Init();

        /* USER CODE BEGIN 2 */
        initCRC(0); //iNemo CRC False
        initCRC(1); //Driver CRC XModem
        SetupArrays();
        //SetupMotors();
        /* USER CODE END 2 */

        /* USER CODE BEGIN RTOS_MUTEX */
        /* add mutexes, ... */
        /* USER CODE END RTOS_MUTEX */

        /* USER CODE BEGIN RTOS_SEMAPHORES */
        /* add semaphores, ... */
        /* USER CODE END RTOS_SEMAPHORES */

        /* USER CODE BEGIN RTOS_TIMERS */
        /* start timers, add new ones, ... */
        /* USER CODE END RTOS_TIMERS */

        /* Create the thread(s) */
        /* definition and creation of defaultTask */
        osThreadDef(defaultTask, StartDefaultTask, osPriorityRealtime, 0, 128);
        defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

        /* definition and creation of TXPC */
        osThreadDef(TXPC, StartTXPC, osPriorityRealtime, 0, 128);
        TXPCHandle = osThreadCreate(osThread(TXPC), NULL);

        /* definition and creation of RXPC */
        osThreadDef(RXPC, StartRXPC, osPriorityRealtime, 0, 128);
        RXPCHandle = osThreadCreate(osThread(RXPC), NULL);

        /* definition and creation of Heartbeat */
        osThreadDef(Heartbeat, StartHeartbeat, osPriorityRealtime, 0, 128);
        HeartbeatHandle = osThreadCreate(osThread(Heartbeat), NULL);

        /* definition and creation of TXMotor1 */
        osThreadDef(TXMotor1, StartTXMotor1, osPriorityHigh, 0, 128);
        TXMotor1Handle = osThreadCreate(osThread(TXMotor1), NULL);

        /* definition and creation of TXMotor2 */
        osThreadDef(TXMotor2, StartTXMotor2, osPriorityHigh, 0, 128);
        TXMotor2Handle = osThreadCreate(osThread(TXMotor2), NULL);

        /* definition and creation of RXMotor1 */
        osThreadDef(RXMotor1, StartRXMotor1, osPriorityAboveNormal, 0, 128);
        RXMotor1Handle = osThreadCreate(osThread(RXMotor1), NULL);

        /* definition and creation of RXMotor2 */
        osThreadDef(RXMotor2, StartRXMotor2, osPriorityAboveNormal, 0, 128);
        RXMotor2Handle = osThreadCreate(osThread(RXMotor2), NULL);

        /* USER CODE BEGIN RTOS_THREADS */
        /* add threads, ... */
        /* USER CODE END RTOS_THREADS */

        /* Create the queue(s) */
        /* definition and creation of ProcessQM1 */
        osMessageQDef(ProcessQM1, 1, uint32_t);
        ProcessQM1Handle = osMessageCreate(osMessageQ(ProcessQM1), NULL);

        /* definition and creation of ProcessQM2 */
        osMessageQDef(ProcessQM2, 1, uint32_t);
        ProcessQM2Handle = osMessageCreate(osMessageQ(ProcessQM2), NULL);

        /* definition and creation of TransmitQ */
        osMessageQDef(TransmitQ, 20, uint32_t);
        TransmitQHandle = osMessageCreate(osMessageQ(TransmitQ), NULL);

        /* definition and creation of ProcessQiNemo */
        osMessageQDef(ProcessQiNemo, 1, uint32_t);
        ProcessQiNemoHandle = osMessageCreate(osMessageQ(ProcessQiNemo), NULL);

        /* definition and creation of ProcessQPC */
        osMessageQDef(ProcessQPC, 1, uint32_t);
        ProcessQPCHandle = osMessageCreate(osMessageQ(ProcessQPC), NULL);

        /* definition and creation of TransmitM1Q */
        osMessageQDef(TransmitM1Q, 1, uint32_t);
        TransmitM1QHandle = osMessageCreate(osMessageQ(TransmitM1Q), NULL);

        /* definition and creation of TransmitM2Q */
        osMessageQDef(TransmitM2Q, 1, uint32_t);
        TransmitM2QHandle = osMessageCreate(osMessageQ(TransmitM2Q), NULL);

        /* USER CODE BEGIN RTOS_QUEUES */
        /* add queues, ... */
        /* USER CODE END RTOS_QUEUES */


        /* Start scheduler */
        osKernelStart();

        /* We should never get here as control is now taken by the scheduler */

        /* Infinite loop */
        /* USER CODE BEGIN WHILE */
        while (1)
        {
                /* USER CODE END WHILE */

                /* USER CODE BEGIN 3 */

        }
        /* USER CODE END 3 */

}

/** System Clock Configuration
 */
void SystemClock_Config(void)
{

        RCC_OscInitTypeDef RCC_OscInitStruct;
        RCC_ClkInitTypeDef RCC_ClkInitStruct;

        __HAL_RCC_PWR_CLK_ENABLE();

        __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState = RCC_HSI_ON;
        RCC_OscInitStruct.HSICalibrationValue = 16;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        {
                Error_Handler();
        }

        RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                      |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
        RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
        {
                Error_Handler();
        }

        HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

        HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

        /* SysTick_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
}

/* USART1 init function */
static void MX_USART1_UART_Init(void)
{

        huart1.Instance = USART1;
        huart1.Init.BaudRate = 115200;
        huart1.Init.WordLength = UART_WORDLENGTH_8B;
        huart1.Init.StopBits = UART_STOPBITS_1;
        huart1.Init.Parity = UART_PARITY_NONE;
        huart1.Init.Mode = UART_MODE_TX_RX;
        huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart1.Init.OverSampling = UART_OVERSAMPLING_16;
        if (HAL_UART_Init(&huart1) != HAL_OK)
        {
                Error_Handler();
        }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

        huart2.Instance = USART2;
        huart2.Init.BaudRate = 921600;
        huart2.Init.WordLength = UART_WORDLENGTH_8B;
        huart2.Init.StopBits = UART_STOPBITS_1;
        huart2.Init.Parity = UART_PARITY_NONE;
        huart2.Init.Mode = UART_MODE_TX_RX;
        huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart2.Init.OverSampling = UART_OVERSAMPLING_16;
        if (HAL_UART_Init(&huart2) != HAL_OK)
        {
                Error_Handler();
        }

}

/* USART3 init function */
static void MX_USART3_UART_Init(void)
{

        huart3.Instance = USART3;
        huart3.Init.BaudRate = 921600;
        huart3.Init.WordLength = UART_WORDLENGTH_8B;
        huart3.Init.StopBits = UART_STOPBITS_1;
        huart3.Init.Parity = UART_PARITY_NONE;
        huart3.Init.Mode = UART_MODE_TX_RX;
        huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart3.Init.OverSampling = UART_OVERSAMPLING_16;
        if (HAL_UART_Init(&huart3) != HAL_OK)
        {
                Error_Handler();
        }

}

/* USART6 init function */
static void MX_USART6_UART_Init(void)
{

        huart6.Instance = USART6;
        huart6.Init.BaudRate = 115200;
        huart6.Init.WordLength = UART_WORDLENGTH_8B;
        huart6.Init.StopBits = UART_STOPBITS_1;
        huart6.Init.Parity = UART_PARITY_NONE;
        huart6.Init.Mode = UART_MODE_TX_RX;
        huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart6.Init.OverSampling = UART_OVERSAMPLING_16;
        if (HAL_UART_Init(&huart6) != HAL_OK)
        {
                Error_Handler();
        }

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{
        /* DMA controller clock enable */
        __HAL_RCC_DMA2_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        /* DMA interrupt init */
        /* DMA1_Stream1_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
        /* DMA1_Stream3_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
        /* DMA1_Stream5_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
        /* DMA1_Stream6_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
        /* DMA2_Stream1_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
        /* DMA2_Stream2_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
        /* DMA2_Stream7_IRQn interrupt configuration */
        HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

}

/** Configure pins as
 * Analog
 * Input
 * Output
 * EVENT_OUT
 * EXTI
 */
static void MX_GPIO_Init(void)
{

        GPIO_InitTypeDef GPIO_InitStruct;

        /* GPIO Ports Clock Enable */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /*Configure GPIO pin Output Level */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

        /*Configure GPIO pin : PB9 */
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

void SetupMotors(void){
        //Initialize Motors
        TransmitM1_DMA(WRITE, sizeof(WRITE));
        while(huart2.gState != HAL_UART_STATE_READY);
        TransmitM2_DMA(WRITE, sizeof(WRITE));
        while(huart3.gState != HAL_UART_STATE_READY);
        TransmitM1_DMA(BRIDGE, sizeof(BRIDGE));
        while(huart2.gState != HAL_UART_STATE_READY);
        TransmitM2_DMA(BRIDGE, sizeof(BRIDGE));
        while(huart3.gState != HAL_UART_STATE_READY);
}

void SetupArrays(void){
        //Initialize arrays
        memccpy(MotorPacket.KILL, KILL, 0, sizeof(KILL));
        memccpy(MotorPacket.WRITE, WRITE, 0, sizeof(WRITE));
        memccpy(MotorPacket.BRIDGE, BRIDGE, 0, sizeof(BRIDGE));
        memccpy(MotorPacket.CURRENT_SET, CURRENT_SET, 0, sizeof(CURRENT_SET));
        memccpy(MotorPacket.CURRENT_DATA, CURRENT_DATA, 0, sizeof(CURRENT_DATA));
        memccpy(MotorPacket.POSITION_DATA, POSITION_DATA, 0, sizeof(POSITION_DATA));
        memccpy(MotorPacket.VELOCITY_DATA, VELOCITY_DATA, 0, sizeof(VELOCITY_DATA));

        MotorPacketSize.KILL = KILL_L;
        MotorPacketSize.WRITE = WRITE_L;
        MotorPacketSize.BRIDGE = BRIDGE_L;
        MotorPacketSize.CURRENT_SET = CURRENT_SET_L;
        MotorPacketSize.CURRENT_DATA = CURRENT_DATA_L;
        MotorPacketSize.POSITION_DATA = POSITION_DATA_L;
        MotorPacketSize.VELOCITY_DATA = VELOCITY_DATA_L;
}

//void SetCurrent(uint8_t buf[]){
//        SetBuf(buf, 14, Current_Command, 14);
//        SetBytes(buf, 8, RXBuf[0], 9, RXBuf[1], 10, RXBuf[2], 11, RXBuf[3]);
//}

void ClearBuf(uint8_t buf[], uint8_t size){
        uint8_t i;
        for (i = 0; i < size; i++) {
                buf[i] = 0;
        }
}

void SetBuf(uint8_t buf[], uint8_t set[], uint8_t size){
        //ClearBuf(buf, 14);
        uint8_t i;
        for (i = 0; i < size; i++) {
                buf[i] = set[i];
        }
}

void SetBytes(uint8_t buf[], uint8_t pos1, uint8_t val1, uint8_t pos2, uint8_t val2, uint8_t pos3, uint8_t val3, uint8_t pos4, uint8_t val4){
        if (pos1 != -1) {buf[pos1] = buf[val1]; }
        if (pos2 != -1) {buf[pos2] = buf[val2]; }
        if (pos3 != -1) {buf[pos3] = buf[val3]; }
        if (pos4 != -1) {buf[pos4] = buf[val4]; }
}

void TransmitM1_DMA(uint8_t *data, uint8_t size){
        /* Start the transmission - an interrupt is generated when the transmission
           is complete. */
        //if(HAL_UART_Transmit_DMA(&huart2, data, size) != HAL_OK) { Error_Handler(); }
        HAL_UART_Transmit_DMA(&huart2, data, size);
}

void ReceiveM1_DMA(uint8_t *data, uint8_t size){
        //HAL_UART_Receive_DMA(&huart2, data, size);
        HAL_UART_Receive(&huart2, data, size, 5);
}

void TransmitM2_DMA(uint8_t *data, uint8_t size){
        /* Start the transmission - an interrupt is generated when the transmission
           is complete. */
        //if(HAL_UART_Transmit_DMA(&huart3, data, size) != HAL_OK) { Error_Handler(); }
        HAL_UART_Transmit_DMA(&huart3, data, size);
}

void ReceiveM2_DMA(uint8_t *data, uint8_t size){
        //HAL_UART_Receive_DMA(&huart3, data, size);
        HAL_UART_Receive(&huart3, data, size, 5);
}

//Select Call-backs functions called after Transfer complete
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
        __NOP();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
        __NOP();
}

/* USER CODE END 4 */

/* StartDefaultTask function */
void StartDefaultTask(void const * argument)
{

        /* USER CODE BEGIN 5 */
        vTaskSuspend( NULL );
        /* Infinite loop */
        for(;; )
        {
                osDelay(500);
        }
        /* USER CODE END 5 */
}

/* StartTXPC function */
void StartTXPC(void const * argument)
{
        /* USER CODE BEGIN StartTXPC */

        /* Infinite loop */
        for(;; )
        {
                // Suspend ourselves.
                vTaskSuspend( NULL );
                // We cannot get here unless another task calls vTaskResume
                // with our handle as the parameter.
                HAL_UART_Transmit_DMA(&huart1, PCPacketPTR, sizeof(PCPacket));
        }
        /* USER CODE END StartTXPC */
}

/* StartRXPC function */
void StartRXPC(void const * argument)
{
        /* USER CODE BEGIN StartRXPC */
        /* Infinite loop */
        for(;; )
        {

                osDelay(Ts);
        }
        /* USER CODE END StartRXPC */
}

/* StartHeartbeat function */
void StartHeartbeat(void const * argument)
{
        /* USER CODE BEGIN StartHeartbeat */
        //http://www.freertos.org/xEventGroupSync.html
        //For reference: ALL_SYNC_BITS ( TASK_HEARTBEAT | TASK_TXM1 | TASK_TXM2 | TASK_RXM1 | TASK_RXM2 )
        //osMessageQId TransmitM1QHandle;
        //osMessageQId TransmitM2QHandle;
        //Message from the "heart"
        //	struct HeartMessage
        //	 {
        //		  uint8_t ucMessageID;
        //	    uint8_t ucMessageLen;
        //	    uint8_t ucData[ucMessageLen];
        //	 };
        //	struct HeartMessage xHeartM1;
        //	struct HeartMessage xHeartM2;
        //
        //#define KILL (1 << 0)
        //#define WRITE (1 << 1)
        //#define BRIDGE (1 << 2)
        //#define CURRENT_SET (1 << 3)
        //#define CURRENT_DATA (1 << 4)
        //#define POSITION_DATA (1 << 5)
        //#define VELOCITY_DATA (1 << 6)

        EventBits_t uxReturn;
        //TickType_t xTicksToWait = 5 / portTICK_PERIOD_MS;
        TickType_t xTicksToWait = portMAX_DELAY;

        struct HeartMessage *pxMessage;

        /* Attempt to create the event group. */
        xEventSyncDriver = xEventGroupCreate();

        /* Was the event group created successfully? */
        if( xEventSyncDriver == NULL )
        {
                /* The event group was not created because there was insufficient
                   FreeRTOS heap available. */
        }
        else
        {
                /* The event group was created. */
        }

        uint8_t ID = 1; //Start with write enable

        /* Infinite loop */
        for(;; )
        {
                /* Perform task functionality here. */
                pxMessage = &xHeartM1;
                pxMessage->ucMessageID = ID;
                pxMessage->ucData = &MotorPacketPTR;
                xQueueOverwrite( TransmitM1QHandle, ( void * ) &pxMessage);

                pxMessage = &xHeartM2;
                pxMessage->ucMessageID = ID;
                pxMessage->ucData = &MotorPacketPTR;
                xQueueOverwrite( TransmitM2QHandle, ( void * ) &pxMessage);

                //Resume other tasks
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);

                /* Set bit 0 in the event group to note this task has reached the
                   sync point.  The other two tasks will set the other two bits defined
                   by ALL_SYNC_BITS.  All three tasks have reached the synchronisation
                   point when all the ALL_SYNC_BITS are set.  Wait a maximum of 100ms
                   for this to happen. */
                uxReturn = xEventGroupSync( xEventSyncDriver,
                                            TASK_HEARTBEAT,
                                            ALL_SYNC_BITS,
                                            xTicksToWait );
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
                if( ( uxReturn & ALL_SYNC_BITS ) == ALL_SYNC_BITS )
                {
                        /* All three tasks reached the synchronisation point before the call
                           to xEventGroupSync() timed out. */
                        //Overwrite last message

                        ID++;
                        if(ID==6)
                        {
                                // Resume the suspended task ourselves.
                                vTaskResume( TXPCHandle );
                                // The created task will once again get microcontroller processing
                                // time in accordance with its priority within the system.
                                ID = 3; //Start with motor current write
                                osDelay(5);
                                vTaskSuspend( TXPCHandle );
                        }
                }
        }
        /* USER CODE END StartHeartbeat */
}

/* StartTXMotor1 function */
void StartTXMotor1(void const * argument)
{
        /* USER CODE BEGIN StartTXMotor1 */
        struct HeartMessage *pxRxedMessage;

        /* Infinite loop */
        for(;; )
        {
                // Receive a message on the created queue. Block 5.
                xQueueReceive( TransmitM1QHandle, &( pxRxedMessage ), 5);
                //&(pxRxedMessage->ucData)[(pxRxedMessage->ucMessageID)*14]
                TransmitM1_DMA(MotorPacketArray[pxRxedMessage->ucMessageID], MotorPacketSizePTR[pxRxedMessage->ucMessageID]);
                while(huart2.gState != HAL_UART_STATE_READY);

                while(1) {
                        TransmitM1_DMA(MotorPacketArray[pxRxedMessage->ucMessageID], MotorPacketSizePTR[pxRxedMessage->ucMessageID]);
                        while(huart2.gState != HAL_UART_STATE_READY) ;
                        osDelay(100);
                }




                /* Set bit 1 in the event group to note this task has reached the
                        synchronization point.  The other two tasks will set the other two
                        bits defined by ALL_SYNC_BITS.  All three tasks have reached the
                        synchronization point when all the ALL_SYNC_BITS are set.  Wait
                        indefinitely for this to happen. */
                xEventGroupSync( xEventSyncDriver, TASK_TXM1, ALL_SYNC_BITS, portMAX_DELAY );
        }
        /* USER CODE END StartTXMotor1 */
}

/* StartTXMotor2 function */
void StartTXMotor2(void const * argument)
{
        /* USER CODE BEGIN StartTXMotor2 */
        struct HeartMessage *pxRxedMessage;
        /* Infinite loop */
        for(;; )
        {
                // Receive a message on the created queue. Block 5.
                xQueueReceive( TransmitM2QHandle, &( pxRxedMessage ), 5);
                TransmitM1_DMA(MotorPacketArray[pxRxedMessage->ucMessageID], MotorPacketSizePTR[pxRxedMessage->ucMessageID]);
                while(huart3.gState != HAL_UART_STATE_READY);

                /* Set bit 1 in the event group to note this task has reached the
                        synchronization point.  The other two tasks will set the other two
                        bits defined by ALL_SYNC_BITS.  All three tasks have reached the
                        synchronization point when all the ALL_SYNC_BITS are set.  Wait
                        indefinitely for this to happen. */
                xEventGroupSync( xEventSyncDriver, TASK_TXM2, ALL_SYNC_BITS, portMAX_DELAY );
        }
        /* USER CODE END StartTXMotor2 */
}

/* StartRXMotor1 function */
void StartRXMotor1(void const * argument)
{
        /* USER CODE BEGIN StartRXMotor1 */
        uint8_t *ByteM1;

        uint8_t BUFF_SIZE;
        uint8_t DATA_SIZE;
        uint8_t CRC_SIZE;
        uint8_t START_INDEX;
        uint8_t *EXTRACT_DATA;
        uint8_t *EXTRACT_CRC;
        uint32_t CALC_CRC;

        struct HeartMessage *pxRxedMessage;

        /* Infinite loop */
        for(;; )
        {
                // Receive a message on the created queue. Block 5.
                xQueueReceive( TransmitM1QHandle, &( pxRxedMessage ), 5);
                ReceiveM1_DMA(ByteM1, 1);
                //while(huart2.RxState != HAL_UART_STATE_READY);
                while(ByteM1[0]!=0xA5) {
                        ReceiveM1_DMA(ByteM1, 1);
                        //while(huart2.RxState != HAL_UART_STATE_READY);
                }
                while(ByteM1[0]!=0xFF) {
                        ReceiveM1_DMA(ByteM1, 1);
                        //while(huart2.RxState != HAL_UART_STATE_READY);
                }
                ReceiveM1_DMA(ByteM1, 1);
                //while(huart2.RxState != HAL_UART_STATE_READY);

                switch(ByteM1[0]) {
                case 0x04: //Kill Bridge
                        osDelay(2000);
                        __HAL_UART_DISABLE(&huart2);
                        break;
                case 0x08: //Write Enable
                        osDelay(2000);
                        break;
                case 0x10: //Bridge enable
                        osDelay(2000);
                        break;
                case 0x0C: //Current_Set
                        break;
                case 0x32: //Current_Data
                        BUFF_SIZE = 9; //From Command Byte to end CRC
                        ReceiveM1_DMA(RXBufM1, BUFF_SIZE);
                        //while(huart2.RxState != HAL_UART_STATE_READY);
                        DATA_SIZE = 2;
                        CRC_SIZE = 2;
                        START_INDEX = 0;
                        EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 6, DATA_SIZE);
                        EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 8, CRC_SIZE);
                        WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                        WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                        EXTRACT_CRC = WORDtoBYTE.WORD;
                        CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                        if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1C, EXTRACT_DATA, 0, DATA_SIZE); }
                        break;
                case 0x3E: //Position_Data
                        BUFF_SIZE = 11;
                        ReceiveM1_DMA(RXBufM1, BUFF_SIZE);
                        //while(huart2.RxState != HAL_UART_STATE_READY);
                        DATA_SIZE = 4;
                        CRC_SIZE = 2;
                        START_INDEX = 0;
                        EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 6, DATA_SIZE);
                        EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 10, CRC_SIZE);
                        WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                        WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                        EXTRACT_CRC = WORDtoBYTE.WORD;
                        CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                        if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1P, EXTRACT_DATA, 0, DATA_SIZE); }
                        break;
                case 0x16: //Velocity_Data
                        BUFF_SIZE = 11;
                        ReceiveM1_DMA(RXBufM1, BUFF_SIZE);
                        //while(huart2.RxState != HAL_UART_STATE_READY);
                        DATA_SIZE = 4;
                        CRC_SIZE = 2;
                        START_INDEX = 0;
                        EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 6, DATA_SIZE);
                        EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 10, CRC_SIZE);
                        WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                        WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                        EXTRACT_CRC = WORDtoBYTE.WORD;
                        CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                        if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1V, EXTRACT_DATA, 0, DATA_SIZE); }
                        break;
                default:
                        break;
                }

                memset(RXBufM1, 0, 20);

                //Rx A5 Rx FF Rx CMD -> Check bits 2-5 for opcode -> Move on to specific Rx
                //worst case TX blocked for 5ms and new transmission takes place, reply should be picked up
                // Command Byte (byte 3 of packet) Opcode: xx xx:
                // TX->Reply
                // Set Current (0)
                // 00xx xx10->00xx xx00
                // 0000 1110->0000 1100
                // 0x0E->0x0C
                // Read Current (2)
                // 00xx xx01->00xx xx10
                // 0011 0001->0011 0010
                // 0x31->0x32
                // Read Velocity (4)
                // 00xx xx01->00xx xx10
                // 0011 1101->0011 1110
                // 0x3D->0x3E
                // Read Position (4)
                // 00xx (2)xx(1) 01->00xx xx10
                // 0001 0101->0001 0110
                // 0x15->0x16

                //struct HeartMessage *pxRxedMessage;
                // Receive a message on the created queue.
                // xQueueReceive( TransmitM1QHandle, &( pxRxedMessage ), 20);
                // ReceiveM1_DMA(RXBufM1, MotorPacketSizePTR[pxRxedMessage->ucMessageID]);
                // //TODO wait
                //
                // switch(pxRxedMessage->ucMessageID) {
                // case ID_KILL:
                //         osDelay(2000);
                //         __HAL_UART_DISABLE(&huart2);
                //         break;
                // case ID_WRITE:
                //         osDelay(2000);
                //         break;
                // case ID_BRIDGE:
                //         osDelay(2000);
                //         break;
                // case ID_CURRENT_SET:
                //         break;
                // case ID_CURRENT_DATA:
                //         BUFF_SIZE = 20;
                //         START_BYTE[0] = 0xA5;
                //         START_BYTE[1] = 0xFF;
                //         START_SIZE = 2;
                //         DATA_SIZE = 2;
                //         CRC_SIZE = 2;
                //         START_INDEX = findBytes(RXBufM1, BUFF_SIZE, START_BYTE, START_SIZE, 1);
                //         EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 8, DATA_SIZE);
                //         EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 10, CRC_SIZE);
                //         WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                //         WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                //         EXTRACT_CRC = WORDtoBYTE.WORD;
                //         CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                //         if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1C, EXTRACT_DATA, 0, DATA_SIZE); }
                //         break;
                // case ID_POSITION_DATA:
                //         BUFF_SIZE = 20;
                //         START_BYTE[0] = 0xA5;
                //         START_BYTE[1] = 0xFF;
                //         START_SIZE = 2;
                //         DATA_SIZE = 4;
                //         CRC_SIZE = 2;
                //         START_INDEX = findBytes(RXBufM1, BUFF_SIZE, START_BYTE, START_SIZE, 1);
                //         EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 8, DATA_SIZE);
                //         EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 12, 2);
                //         WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                //         WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                //         EXTRACT_CRC = WORDtoBYTE.WORD;
                //         CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                //         if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1P, EXTRACT_DATA, 0, DATA_SIZE); }
                //         break;
                // case ID_VELOCITY_DATA:
                //         BUFF_SIZE = 20;
                //         START_BYTE[0] = 0xA5;
                //         START_BYTE[1] = 0xFF;
                //         START_SIZE = 2;
                //         DATA_SIZE = 4;
                //         CRC_SIZE = 2;
                //         START_INDEX = findBytes(RXBufM1, BUFF_SIZE, START_BYTE, START_SIZE, 1);
                //         EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 8, DATA_SIZE);
                //         EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 12, CRC_SIZE);
                //         WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                //         WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                //         EXTRACT_CRC = WORDtoBYTE.WORD;
                //         CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                //         if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1V, EXTRACT_DATA, 0, DATA_SIZE); }
                //         break;
                // default:
                //         break;
                // };
                //

                /* Set bit 1 in the event group to note this task has reached the
                        synchronization point.  The other two tasks will set the other two
                        bits defined by ALL_SYNC_BITS.  All three tasks have reached the
                        synchronization point when all the ALL_SYNC_BITS are set.  Wait
                        indefinitely for this to happen. */
                xEventGroupSync( xEventSyncDriver, TASK_RXM1, ALL_SYNC_BITS, portMAX_DELAY );
        }
        /* USER CODE END StartRXMotor1 */
}

/* StartRXMotor2 function */
void StartRXMotor2(void const * argument)
{
        /* USER CODE BEGIN StartRXMotor2 */
        uint8_t *ByteM2;

        uint8_t BUFF_SIZE;
        uint8_t DATA_SIZE;
        uint8_t CRC_SIZE;
        uint8_t START_INDEX;
        uint8_t *EXTRACT_DATA;
        uint8_t *EXTRACT_CRC;
        uint32_t CALC_CRC;

        struct HeartMessage *pxRxedMessage;

        /* Infinite loop */
        for(;; )
        {
                // Receive a message on the created queue. Block 5.
                xQueueReceive( TransmitM2QHandle, &( pxRxedMessage ), 5);
                ReceiveM2_DMA(ByteM2, 1);
                //while(huart3.RxState != HAL_UART_STATE_READY);
                while(ByteM2[0]!=0xA5) {
                        ReceiveM2_DMA(ByteM2, 1);
                        //while(huart3.RxState != HAL_UART_STATE_READY);
                }
                while(ByteM2[0]!=0xFF) {
                        ReceiveM2_DMA(ByteM2, 1);
                        //while(huart3.RxState != HAL_UART_STATE_READY);
                }
                ReceiveM2_DMA(ByteM2, 1);
                //while(huart3.RxState != HAL_UART_STATE_READY);

                switch(ByteM2[0]) {
                case 0x04: //Kill Bridge
                        osDelay(2000);
                        __HAL_UART_DISABLE(&huart3);
                        break;
                case 0x08: //Write Enable
                        osDelay(2000);
                        break;
                case 0x10: //Bridge enable
                        osDelay(2000);
                        break;
                case 0x0C: //Current_Set
                        break;
                case 0x32: //Current_Data
                        BUFF_SIZE = 9; //From Command Byte to end CRC
                        ReceiveM2_DMA(RXBufM2, BUFF_SIZE);
                        //while(huart3.RxState != HAL_UART_STATE_READY);
                        DATA_SIZE = 2;
                        CRC_SIZE = 2;
                        START_INDEX = 0;
                        EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 6, DATA_SIZE);
                        EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 8, CRC_SIZE);
                        WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                        WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                        EXTRACT_CRC = WORDtoBYTE.WORD;
                        CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                        if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1C, EXTRACT_DATA, 0, DATA_SIZE); }
                        break;
                case 0x3E: //Position_Data
                        BUFF_SIZE = 11;
                        ReceiveM2_DMA(RXBufM2, BUFF_SIZE);
                        //while(huart3.RxState != HAL_UART_STATE_READY);
                        DATA_SIZE = 4;
                        CRC_SIZE = 2;
                        START_INDEX = 0;
                        EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 6, DATA_SIZE);
                        EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 10, CRC_SIZE);
                        WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                        WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                        EXTRACT_CRC = WORDtoBYTE.WORD;
                        CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                        if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1P, EXTRACT_DATA, 0, DATA_SIZE); }
                        break;
                case 0x16: //Velocity_Data
                        BUFF_SIZE = 11;
                        ReceiveM2_DMA(RXBufM2, BUFF_SIZE);
                        //while(huart3.RxState != HAL_UART_STATE_READY);
                        DATA_SIZE = 4;
                        CRC_SIZE = 2;
                        START_INDEX = 0;
                        EXTRACT_DATA = extractBytes(RXBufM1, START_INDEX + 6, DATA_SIZE);
                        EXTRACT_CRC = extractBytes(RXBufM1, START_INDEX + 10, CRC_SIZE);
                        WORDtoBYTE.BYTE[1] = EXTRACT_CRC[0];
                        WORDtoBYTE.BYTE[0] = EXTRACT_CRC[1];
                        EXTRACT_CRC = WORDtoBYTE.WORD;
                        CALC_CRC = crcCalc(EXTRACT_DATA, 0, DATA_SIZE, 1);
                        if(EXTRACT_CRC==CALC_CRC) {memccpy(PCPacket.M1V, EXTRACT_DATA, 0, DATA_SIZE); }
                        break;
                default:
                        break;
                }

                memset(RXBufM2, 0, 20);

                /* Set bit 1 in the event group to note this task has reached the
                        synchronization point.  The other two tasks will set the other two
                        bits defined by ALL_SYNC_BITS.  All three tasks have reached the
                        synchronization point when all the ALL_SYNC_BITS are set.  Wait
                        indefinitely for this to happen. */
                xEventGroupSync( xEventSyncDriver, TASK_RXM2, ALL_SYNC_BITS, portMAX_DELAY );
        }
        /* USER CODE END StartRXMotor2 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM1 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
/* USER CODE BEGIN Callback 0 */

/* USER CODE END Callback 0 */
        if (htim->Instance == TIM1) {
                HAL_IncTick();
        }
/* USER CODE BEGIN Callback 1 */

/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
void Error_Handler(void)
{
        /* USER CODE BEGIN Error_Handler */
        /* User can add his own implementation to report the HAL error return state */
        while(1)
        {
        }
        /* USER CODE END Error_Handler */
}

#ifdef USE_FULL_ASSERT

/**
 * @brief Reports the name of the source file and the source line number
 * where the assert_param error has occurred.
 * @param file: pointer to the source file name
 * @param line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
        /* USER CODE BEGIN 6 */
        /* User can add his own implementation to report the file name and line number,
           ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
        /* USER CODE END 6 */

}

#endif

/**
 * @}
 */

/**
 * @}
 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
