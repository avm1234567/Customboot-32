/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"

#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;

#define APP_ADDRESS1 (uint32_t)0x08004000
//#define APP_ADDRESS2 (uint32_t)0x08006000
GPIO_PinState pinState;
#define RX_CHUNK_SIZE 1024
uint8_t rx_buffer[RX_CHUNK_SIZE];
#define CHUNK_SIZE 520

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART1_UART_Init(void);

static void goto_app(uint32_t addr);
static void JumpToAddress(uint32_t addr);
uint8_t EraseUserApplication(uint32_t addr);
uint8_t WriteUserApplication(uint32_t addr, uint32_t* data, uint32_t dataSize, uint32_t offset);
uint8_t UserApplicationExists(uint32_t addr);
void ReceiveChunkOverUART(uint32_t addr, const char* str);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
//uint8_t check = "LOG\n";
  while (1)
  {
//	  for(uint8_t i = 0; i < 5; i++){
//		    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//
//		  HAL_Delay(500);
//	  }
	  //printf("Calling ReceiveChunkOverUART...\n");
	  pinState = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
	  if(pinState == GPIO_PIN_RESET){
	        ReceiveChunkOverUART(APP_ADDRESS1, "Send_A\r");
	  }
	  else if(pinState == GPIO_PIN_SET){
	        ReceiveChunkOverUART(APP_ADDRESS1, "Send_B\r");

	  }// blocks forever or jumps
//	        printf("Should never print this\n");
//	  ReceiveChunkOverUART(APP_ADDRESS1);
// Infinite loop
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

void MX_USART1_UART_Init(void)
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

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

//#ifdef __GNUC__
//int __io_putchar(int ch)
//#else
//int fputc(int ch, FILE *f)
//#endif
//{
//  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
//  return ch;
//}

typedef void (*pFunction)(void);

void JumpToAddress(uint32_t addr) {
    uint32_t JumpAddress = *(uint32_t *) (addr + 4);
    pFunction Jump = (pFunction) JumpAddress;

    HAL_RCC_DeInit();
    HAL_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    SCB->VTOR = addr;
    __set_MSP(*(uint32_t *) addr);

    Jump();
}

void goto_app(uint32_t addr) {
    JumpToAddress(addr);
}

uint8_t EraseUserApplication(uint32_t addr) {
    HAL_StatusTypeDef success = HAL_ERROR;
    uint32_t errorSector = 0;

    if (HAL_FLASH_Unlock() == HAL_OK) {
        FLASH_EraseInitTypeDef eraseInit = {0};
        eraseInit.TypeErase   = FLASH_TYPEERASE_PAGES;
        eraseInit.PageAddress = addr;
        eraseInit.NbPages     = 8;
       // eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        success = HAL_FLASHEx_Erase(&eraseInit, &errorSector);

        HAL_FLASH_Lock();
    }

    return success == HAL_OK ? 1 : 0;
}

uint8_t WriteUserApplication(uint32_t addr, uint32_t* data, uint32_t dataSize, uint32_t offset) {
    if (HAL_FLASH_Unlock() == HAL_OK) {
        for (int i = 0; i < dataSize; i++) {
            HAL_StatusTypeDef success = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + offset + (i * 4), data[i]);

            if (success != HAL_OK) {
//                HAL_UART_Transmit(&huart1, "Received shit\r\n", strlen("Received shit\r\n"), HAL_MAX_DELAY);

                HAL_FLASH_Lock();
                return 0;
            }
        }
        HAL_FLASH_Lock();
    } else {
        return 0;
    }

    return 1;
}

uint8_t UserApplicationExists(uint32_t addr) {
    uint32_t bootloaderMspValue = *(uint32_t *) (0x08000000);
    uint32_t appMspValue = *(uint32_t *) (addr);

    return appMspValue == bootloaderMspValue ? 1 : 0;
}

void ReceiveChunkOverUART(uint32_t addr, const char* str)
{
	EraseUserApplication(addr);
//    const char readyMsg[] = str;
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);

    uint32_t offset = 0;
    while(1){
        memset(rx_buffer, 0, CHUNK_SIZE);
        HAL_UART_Receive(&huart1, rx_buffer, CHUNK_SIZE, HAL_MAX_DELAY);
//        HAL_UART_Transmit(&huart1, "Receiving chunk\r\n", strlen("Receiving chunk\r\n"), HAL_MAX_DELAY);

        if(rx_buffer[CHUNK_SIZE -1] == 0xAF){
            WriteUserApplication((uint32_t)addr, (uint32_t*)&rx_buffer[3], (CHUNK_SIZE-8)/4, offset);
//            HAL_UART_Transmit(&huart1, "Received final chunk\r\n", strlen("Received final chunk\r\n"), HAL_MAX_DELAY);

            goto_app((uint32_t)addr);
        } else if(rx_buffer[CHUNK_SIZE -1] == 0x00){

            WriteUserApplication((uint32_t)addr, (uint32_t*)&rx_buffer[3], (CHUNK_SIZE-8)/4, offset);

            offset += 512;
        }
    }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(100);
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  // Implement custom assertion fail handling if needed
}
#endif /* USE_FULL_ASSERT */
