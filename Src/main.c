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
#include <string.h>
#include <stdio.h>

#include "CRC.h"
//#include "serial_terminal.h"

//https://github.com/PetteriAimonen/Baselibc
//#include "memccpy.c"


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

uint8_t MW[2];
uint8_t MB[2];

typedef struct {                                 // Message object structure
        float current[2];
        float velocity[2];
        float position[2];
        float dist;
        float acc[3];
        float gyr[3];
        uint8_t MW[2]; //Motor write enabled
        uint8_t MB[2]; //Motor bridge enabled
} T_MEAS;

T_MEAS    *mptr;
T_MEAS    *rptr;

//PC Buffer
uint8_t TXBuf[10];
uint8_t RXBuf[10];

//Motor Buffer
uint8_t TXBufM1[14];
uint8_t TXBufM2[14];
uint8_t TXM1Complete=0;
uint8_t TXM2Complete=0;

uint8_t RXBufM1[14];
uint8_t RXBufM2[14];

unsigned char Motor_Success[8] = {0xA5, 0xFF, 0x00, 0x01, 0x00, 0x00, 0xCF, 0xB6};
//These arrays hold all the necessary hex commands to send to motor controllers
uint8_t Write_Access[12];
uint8_t Enable_Bridge[12];
uint8_t Disable_Bridge[12];

uint8_t Current_Command[14];
uint8_t Position_Command[14];

uint8_t Read_Current[8];
uint8_t Read_Velocity[8];
uint8_t Read_Position[8];

uint8_t test = 0;

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
osPoolDef(mpool, 16, T_MEAS);                    // Define memory pool
osPoolId mpool;

//Select Call-backs functions called after Transfer complete
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

//Packet Protocol ############################################################
//'packet' makes sure compiler won't insert any gaps!
struct __attribute__((__packed__)) PCPacketStruct {
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

struct PCPacketStruct PCPacket;
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
        uint8_t KILL[12];
        uint8_t WRITE[12];
        uint8_t BRIDGE[12];
        uint8_t CURRENT_SET[14];
        uint8_t CURRENT_DATA[8];
        uint8_t POSITION_DATA[8];
        uint8_t VELOCITY_DATA[8];
};

struct HeartPacketStruct MotorPacket;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *MotorPacketPTR = (uint8_t*)&MotorPacket;

uint8_t KILL[12] = {0xA5, 0x3F, 0x02, 0x01, 0x00, 0x01, 0x01, 0x47, 0x01, 0x00, 0x33, 0x31};
uint8_t WRITE[12] = {0xA5, 0x3F, 0x02, 0x07, 0x00, 0x01, 0xB3, 0xE7, 0x0F, 0x00, 0x10, 0x3E};
uint8_t BRIDGE[12] = {0xA5, 0x3F, 0x02, 0x01, 0x00, 0x01, 0x01, 0x47, 0x00, 0x00, 0x00, 0x00};
uint8_t CURRENT_SET[14] = {0xA5, 0x3F, 0x02, 0x45, 0x00, 0x02, 0xF0, 0x49, 0x48 /*Current[3]*/, 0x01 /*Current[2]*/, 0x00 /*Current[1]*/, 0x00 /*Current[0]*/, 0xDC, 0x6F}; //TODO Set Current 0
uint8_t CURRENT_DATA[8] = {0xA5, 0x3F, 0x01, 0x10, 0x03, 0x01, 0xBB, 0x9B};
uint8_t POSITION_DATA[8] = {0xA5, 0x3F, 0x01, 0x12, 0x00, 0x02, 0xB0, 0xCB};
uint8_t VELOCITY_DATA[8] = {0xA5, 0x3F, 0x01, 0x11, 0x02, 0x02, 0x8F, 0xF9};

//Initialize arrays

//memccpy(MotorPacket.KILL, KILL, 0, sizeof(KILL));
//memccpy(MotorPacket.WRITE, WRITE, 0, sizeof(WRITE));
//memccpy(MotorPacket.BRIDGE, BRIDGE, 0, sizeof(BRIDGE));
//memccpy(MotorPacket.CURRENT_SET, CURRENT_SET, 0, sizeof(CURRENT_SET));
//memccpy(MotorPacket.CURRENT_DATA, CURRENT_DATA, 0, sizeof(CURRENT_DATA));
//memccpy(MotorPacket.POSITION_DATA, POSITION_DATA, 0, sizeof(POSITION_DATA));
//memccpy(MotorPacket.VELOCITY_DATA, VELOCITY_DATA, 0, sizeof(VELOCITY_DATA));

//Motor Replies
//'packed' makes sure compiler won't insert any gaps!
struct __attribute__((__packed__)) HeartReplyStruct {
        uint8_t KILL_R[8];
        uint8_t WRITE_R[8];
        uint8_t BRIDGE_R[8];
        uint8_t CURRENT_SET_R[8];
        uint8_t CURRENT_DATA_R[12];
        uint8_t POSITION_DATA_R[14];
        uint8_t VELOCITY_DATA_R[14];
};

struct HeartReplyStruct MotorReply;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *MotorReplyPTR = (uint8_t*)&MotorReply;

//TODO
uint8_t KILL_R[12] = {0xA5, 0x3F, 0x02, 0x01, 0x00, 0x01, 0x01, 0x47, 0x01, 0x00, 0x33, 0x31};
uint8_t WRITE_R[12] = {0xA5, 0x3F, 0x02, 0x07, 0x00, 0x01, 0xB3, 0xE7, 0x0F, 0x00, 0x10, 0x3E};
uint8_t BRIDGE_R[12] = {0xA5, 0x3F, 0x02, 0x01, 0x00, 0x01, 0x01, 0x47, 0x00, 0x00, 0x00, 0x00};
uint8_t CURRENT_SET_R[14] = {0xA5, 0x3F, 0x02, 0x45, 0x00, 0x02, 0xF0, 0x49, 0x48 /*Current[3]*/, 0x01 /*Current[2]*/, 0x00 /*Current[1]*/, 0x00 /*Current[0]*/, 0xDC, 0x6F}; //TODO Set Current 0
uint8_t CURRENT_DATA_R[8] = {0xA5, 0x3F, 0x01, 0x10, 0x03, 0x01, 0xBB, 0x9B};
uint8_t POSITION_DATA_R[8] = {0xA5, 0x3F, 0x01, 0x12, 0x00, 0x02, 0xB0, 0xCB};
uint8_t VELOCITY_DATA_R[8] = {0xA5, 0x3F, 0x01, 0x11, 0x02, 0x02, 0x8F, 0xF9};

struct __attribute__((__packed__)) HeartStruct {
        struct __attribute__((__packed__)) KILL {
                uint8_t TX_HEADER[6];
                uint8_t TX_CRC1[2];
                uint8_t TX_DATA[2];
                uint8_t TX_CRC2[2];
                uint8_t RX_HEADER[6];
                uint8_t RX_CRC1[2];
        } KILL;
        struct __attribute__((__packed__)) WRITE {
                uint8_t TX_HEADER[6];
                uint8_t TX_CRC1[2];
                uint8_t TX_DATA[2];
                uint8_t TX_CRC2[2];
                uint8_t RX_HEADER[6];
                uint8_t RX_CRC1[2];
        } WRITE;
        struct __attribute__((__packed__)) BRIDGE {
                uint8_t TX_HEADER[6];
                uint8_t TX_CRC1[2];
                uint8_t TX_DATA[2];
                uint8_t TX_CRC2[2];
                uint8_t RX_HEADER[6];
                uint8_t RX_CRC1[2];
        } BRIDGE;
        struct __attribute__((__packed__)) CURRENT_SET {
                uint8_t TX_HEADER[6];
                uint8_t TX_CRC1[2];
                uint8_t TX_DATA[4];
                uint8_t TX_CRC2[2];
                uint8_t RX_HEADER[6];
                uint8_t RX_CRC1[2];
        } CURRENT_SET;
        struct __attribute__((__packed__)) CURRENT_DATA {
                uint8_t TX_HEADER[6];
                uint8_t TX_CRC1[2];
                uint8_t RX_HEADER[6];
                uint8_t RX_CRC1[2];
                uint8_t RX_DATA[2];
                uint8_t RX_CRC2[2];
        } CURRENT_DATA;
        struct __attribute__((__packed__)) POSITION_DATA {
                uint8_t TX_HEADER[6];
                uint8_t TX_CRC1[2];
                uint8_t RX_HEADER[6];
                uint8_t RX_CRC1[2];
                uint8_t RX_DATA[4];
                uint8_t RX_CRC2[2];
        } POSITION_DATA;
        struct __attribute__((__packed__)) VELOCITY_DATA {
                uint8_t TX_HEADER[6];
                uint8_t TX_CRC1[2];
                uint8_t RX_HEADER[6];
                uint8_t RX_CRC1[2];
                uint8_t RX_DATA[4];
                uint8_t RX_CRC2[2];
        } VELOCITY_DATA;
};

struct HeartStruct MotorHeart;
//Transmit pointer PCPacketPTR with sizeof(PCPacket)
uint8_t *MotorHeartPTR = (uint8_t*)&MotorHeart;

//memccpy(MotorHeart.KILL.TX_HEADER, test, 0, 2);

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
void Motor_Kill(void);

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
        //initCRC();
        //Motor_Commands();
        SetupMotors();
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

void Motor_Kill(void){
        //Disable bridge
        HAL_UART_DMAStop(&huart2);
        HAL_UART_DMAStop(&huart3);

        SetBuf(TXBufM1, Disable_Bridge, 12);
        HAL_UART_Transmit(&huart2, TXBufM1, 14, 100);

        SetBuf(TXBufM2, Disable_Bridge, 12);
        HAL_UART_Transmit(&huart3, TXBufM2, 14, 100);

        __HAL_DMA_DISABLE(&hdma_usart2_tx);
        __HAL_DMA_DISABLE(&hdma_usart3_tx);
}

void SetCurrent(uint8_t buf[]){
        SetBuf(buf, Current_Command, 14);
        SetBytes(buf, 8, RXBuf[0], 9, RXBuf[1], 10, RXBuf[2], 11, RXBuf[3]);
}

// uint8_t Write_Access[11];
// uint8_t Enable_Bridge[11];
// uint8_t Disable_Bridge[20];
//
// uint8_t Current_Command[13];
// uint8_t Position_Command[13];
//
// uint8_t Read_Current[7];
// uint8_t Read_Velocity[7];
// uint8_t Read_Position[7];

// void Motor_Commands(void){
//         Write_Access[0] = 0xA5;
//         Write_Access[1] = 0x3F;
//         Write_Access[2] = 0x02;
//         Write_Access[3] = 0x07;
//         Write_Access[4] = 0x00;
//         Write_Access[5] = 0x01;
//         Write_Access[6] = 0xB3;
//         Write_Access[7] = 0xE7;
//         Write_Access[8] = 0x0F;
//         Write_Access[9] = 0x00;
//         Write_Access[10] = 0x10;
//         Write_Access[11] = 0x3E;
//
//         Enable_Bridge[0] = 0xA5;
//         Enable_Bridge[1] = 0x3F;
//         Enable_Bridge[2] = 0x02;
//         Enable_Bridge[3] = 0x01;
//         Enable_Bridge[4] = 0x00;
//         Enable_Bridge[5] = 0x01;
//         Enable_Bridge[6] = 0x01;
//         Enable_Bridge[7] = 0x47;
//         Enable_Bridge[8] = 0x00;
//         Enable_Bridge[9] = 0x00;
//         Enable_Bridge[10] = 0x00;
//         Enable_Bridge[11] = 0x00;
//
//         Disable_Bridge[0] = 0xA5;
//         Disable_Bridge[1] = 0x3F;
//         Disable_Bridge[2] = 0x02;
//         Disable_Bridge[3] = 0x01;
//         Disable_Bridge[4] = 0x00;
//         Disable_Bridge[5] = 0x01;
//         Disable_Bridge[6] = 0x01;
//         Disable_Bridge[7] = 0x47;
//         Disable_Bridge[8] = 0x01;
//         Disable_Bridge[9] = 0x00;
//         Disable_Bridge[10] = 0x33;
//         Disable_Bridge[11] = 0x31;
//
//         Current_Command[0] = 0xA5;
//         Current_Command[1] = 0x3F;
//         Current_Command[2] = 0x02;
//         Current_Command[3] = 0x45;
//         Current_Command[4] = 0x00;
//         Current_Command[5] = 0x02;
//         Current_Command[6] = 0xF0;
//         Current_Command[7] = 0x49;
//         Current_Command[8] = 0x48; //Data to be set
//         Current_Command[9] = 0x01; //Data to be set
//         Current_Command[10] = 0x00; //Data to be set
//         Current_Command[11] = 0x00; //Data to be set
//         Current_Command[12] = 0xDC;
//         Current_Command[13] = 0x6F;
//
//         Read_Current[0] = 0xA5;
//         Read_Current[1] = 0x3F;
//         Read_Current[2] = 0x01;
//         Read_Current[3] = 0x10;
//         Read_Current[4] = 0x03;
//         Read_Current[5] = 0x01;
//         Read_Current[6] = 0xBB;
//         Read_Current[7] = 0x9B;
//
//         Read_Velocity[0] = 0xA5;
//         Read_Velocity[1] = 0x3F;
//         Read_Velocity[2] = 0x01;
//         Read_Velocity[3] = 0x11;
//         Read_Velocity[4] = 0x02;
//         Read_Velocity[5] = 0x02;
//         Read_Velocity[6] = 0x8F;
//         Read_Velocity[7] = 0xF9;
//
//         Read_Position[0] = 0xA5;
//         Read_Position[1] = 0x3F;
//         Read_Position[2] = 0x01;
//         Read_Position[3] = 0x12;
//         Read_Position[4] = 0x00;
//         Read_Position[5] = 0x02;
//         Read_Position[6] = 0xB0;
//         Read_Position[7] = 0xCB;
// }

void ClearBuf(uint8_t buf[], uint8_t size){
        uint8_t i;
        for (i = 0; i < size; i++) {
                buf[i] = 0;
        }
}

void SetBuf(uint8_t buf[], uint8_t set[], uint8_t size){
        ClearBuf(buf, 14);
        uint8_t i;
        for (i = 0; i < size; i++) {
                buf[i] = set[i];
        }
}

void SetBytes(uint8_t buf[], uint8_t pos1, uint8_t val1, uint8_t pos2, uint8_t val2, uint8_t pos3, uint8_t val3, uint8_t pos4, uint8_t val4){
        if (pos1 != NULL) {buf[pos1] = buf[val1]; }
        if (pos2 != NULL) {buf[pos2] = buf[val2]; }
        if (pos3 != NULL) {buf[pos3] = buf[val3]; }
        if (pos4 != NULL) {buf[pos4] = buf[val4]; }
}

void TransmitM1_DMA(uint8_t *data, uint8_t size){
        /* Start the transmission - an interrupt is generated when the transmission
           is complete. */
        //if(HAL_UART_Transmit_DMA(&huart2, data, size) != HAL_OK) { Error_Handler(); }
        HAL_UART_Transmit_DMA(&huart2, data, size);
}

void ReceiveM1_DMA(uint8_t *data, uint8_t size){
  HAL_UART_Receive_DMA(&huart2, data, size);
}

void TransmitM2_DMA(uint8_t *data, uint8_t size){
        /* Start the transmission - an interrupt is generated when the transmission
           is complete. */
        //if(HAL_UART_Transmit_DMA(&huart3, data, size) != HAL_OK) { Error_Handler(); }
        HAL_UART_Transmit_DMA(&huart3, data, size);
}

void ReceiveM2_DMA(uint8_t *data, uint8_t size){
  HAL_UART_Receive_DMA(&huart3, data, size);
}

//Select Call-backs functions called after Transfer complete
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
        test = 1;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
        test = 1;
}

/* USER CODE END 4 */

/* StartDefaultTask function */
void StartDefaultTask(void const * argument)
{

  /* USER CODE BEGIN 5 */

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

                osDelay(Ts);
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

		osDelay(5000);

        EventBits_t uxReturn;
        TickType_t xTicksToWait = 10 / portTICK_PERIOD_MS;

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

        /* Infinite loop */
        for(;; )
        {
                /* Perform task functionality here. */

        							pxMessage = &xHeartM1;
        	                        pxMessage->ucMessageID = 1;
        	                        pxMessage->ucData = CURRENT_SET;
        	                        xQueueOverwrite( TransmitM1QHandle, ( void * ) &pxMessage);

        	                        pxMessage = &xHeartM2;
        	                        pxMessage->ucMessageID = 0;
        	                        pxMessage->ucData = CURRENT_SET;
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

                }


                osDelay(5);
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
                // Receive a message on the created queue. Don't block.
                xQueueReceive( TransmitM1QHandle, &( pxRxedMessage ), 0);
                TransmitM1_DMA(pxRxedMessage->ucData, 14);
                //TODO strlen(MotorPacketPTR[pxRxedMessage->ucMessageID])
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
	                // Receive a message on the created queue. Don't block.
	                xQueueReceive( TransmitM2QHandle, &( pxRxedMessage ), 0);
	                TransmitM1_DMA(pxRxedMessage->ucData, 14);
	                //TODO strlen(MotorPacketPTR[pxRxedMessage->ucMessageID])
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
  struct HeartMessage *pxRxedMessage;
	        /* Infinite loop */
	        for(;; )
	        {
	                // Receive a message on the created queue. Don't block.
	                xQueueReceive( TransmitM1QHandle, &( pxRxedMessage ), 0);
	                ReceiveM2_DMA(RXBufM1, 12);
	                //TODO strlen(MotorPacketPTR[pxRxedMessage->ucMessageID])
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
  struct HeartMessage *pxRxedMessage;
	        /* Infinite loop */
	        for(;; )
	        {
	                // Receive a message on the created queue. Don't block.
	                xQueueReceive( TransmitM2QHandle, &( pxRxedMessage ), 0);
	                ReceiveM2_DMA(RXBufM2, 12);
	                //TODO strlen(MotorPacketPTR[pxRxedMessage->ucMessageID])
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
