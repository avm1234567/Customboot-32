/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : OTA-Capable Bootloader for STM32
  ******************************************************************************
  * @attention
  *
  * This bootloader communicates with a host processor (like an ESP32) over
  * UART to receive and flash a new application binary.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h> // For memcpy

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
// Define the starting address of the user application in flash memory.
#define APP_ADDRESS 0x08004000
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef void (*pFunction)(void);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// SRAM range for stack pointer validation
#define SRAM_START  0x20000000
#define SRAM_SIZE   (20 * 1024) // 20KB for STM32F103C8
#define SRAM_END    (SRAM_START + SRAM_SIZE)

// OTA Command definitions for UART communication
#define OTA_CMD_START       0xAA // Command from host to start OTA
#define OTA_CMD_FW_INFO     0xAB // Command from host with firmware size and CRC
#define OTA_CMD_FW_DATA     0xAC // Command indicating a firmware data packet
#define OTA_CMD_REBOOT      0xAD // Command from host to reboot after success

#define OTA_ACK             0x06 // Acknowledge byte
#define OTA_NACK            0x15 // Negative Acknowledge byte

#define BOOTLOADER_TIMEOUT  5000 // 5 seconds to wait for OTA command
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
CRC_HandleTypeDef hcrc;

/* USER CODE BEGIN PV */
uint8_t uart_rx_buffer[1]; // Buffer for single-byte UART reception
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_CRC_Init(void);
/* USER CODE BEGIN PFP */
int is_app_valid(void);
void jump_to_application(void);
void handle_ota_update(void);
uint8_t erase_app_flash(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Checks if a valid application exists at APP_ADDRESS.
  */
int is_app_valid()
{
	uint32_t app_sp = *(volatile uint32_t*)APP_ADDRESS;
	return (app_sp >= SRAM_START && app_sp <= SRAM_END);
}

/**
  * @brief  Jumps to the user application.
  */
void jump_to_application()
{
	uint32_t app_reset_handler_address = *(volatile uint32_t*)(APP_ADDRESS + 4);
	pFunction app_reset_handler = (pFunction)app_reset_handler_address;

	HAL_UART_DeInit(&huart1);
	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_13);
    HAL_CRC_DeInit(&hcrc);
	HAL_DeInit();

	SCB->VTOR = APP_ADDRESS;
	__set_MSP(*(volatile uint32_t*)APP_ADDRESS);
	app_reset_handler();
}

/**
 * @brief Erases the application area of the flash memory.
 * @retval 1 on success, 0 on failure.
 */
uint8_t erase_app_flash() {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    // Define which sectors to erase. This depends on the specific STM32 model.
    // For STM32F103C8 (64KB), pages are 1KB. Bootloader is 16KB (16 pages).
    // App starts at 0x08004000, which is page 16.
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = APP_ADDRESS;
    // Erase all pages from APP_ADDRESS to the end of flash.
    // 64KB total flash, 16KB bootloader = 48KB for app. 48 pages.
    EraseInitStruct.NbPages = 48;

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        HAL_FLASH_Lock();
        return 0; // Failure
    }
    HAL_FLASH_Lock();
    return 1; // Success
}

/**
 * @brief Main function to handle the OTA update process.
 */
void handle_ota_update() {
    uint8_t received_cmd;
    uint32_t fw_size = 0;
    uint32_t fw_crc = 0;
    uint32_t bytes_written = 0;
    uint8_t data_buffer[64]; // Buffer for firmware data chunks
    uint32_t calculated_crc;

    // CORRECTED: Create variables for ACK/NACK messages to get valid pointers.
    const uint8_t ack_msg[] = {OTA_ACK};
    const uint8_t nack_msg[] = {OTA_NACK};

    // Send ACK to signal we are in OTA mode
    HAL_UART_Transmit(&huart1, (uint8_t*)ack_msg, 1, HAL_MAX_DELAY);

    // 1. Wait for Firmware Info (Size and CRC)
    if (HAL_UART_Receive(&huart1, &received_cmd, 1, BOOTLOADER_TIMEOUT) != HAL_OK || received_cmd != OTA_CMD_FW_INFO) {
        HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
        return; // Timeout or wrong command
    }
    // Receive 4 bytes for size and 4 bytes for CRC
    uint8_t fw_info_buffer[8];
    if (HAL_UART_Receive(&huart1, fw_info_buffer, 8, BOOTLOADER_TIMEOUT) != HAL_OK) {
        HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
        return;
    }
    memcpy(&fw_size, &fw_info_buffer[0], 4);
    memcpy(&fw_crc, &fw_info_buffer[4], 4);
    HAL_UART_Transmit(&huart1, (uint8_t*)ack_msg, 1, HAL_MAX_DELAY);

    // 2. Erase Flash
    if (!erase_app_flash()) {
        HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
        return; // Erase failed
    }

    // 3. Receive Firmware Data
    HAL_FLASH_Unlock();
    __HAL_CRC_DR_RESET(&hcrc); // Reset CRC calculation unit

    while (bytes_written < fw_size) {
        if (HAL_UART_Receive(&huart1, &received_cmd, 1, BOOTLOADER_TIMEOUT) != HAL_OK || received_cmd != OTA_CMD_FW_DATA) {
            HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
            HAL_FLASH_Lock();
            return;
        }

        // Determine chunk size
        uint16_t chunk_size = (fw_size - bytes_written < sizeof(data_buffer)) ? (fw_size - bytes_written) : sizeof(data_buffer);

        if (HAL_UART_Receive(&huart1, data_buffer, chunk_size, BOOTLOADER_TIMEOUT) != HAL_OK) {
            HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
            HAL_FLASH_Lock();
            return;
        }

        // Write data to flash
        for (int i = 0; i < chunk_size; i += 4) {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_ADDRESS + bytes_written + i, *(uint32_t*)(data_buffer + i)) != HAL_OK) {
                HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
                HAL_FLASH_Lock();
                return;
            }
        }
        bytes_written += chunk_size;
        HAL_UART_Transmit(&huart1, (uint8_t*)ack_msg, 1, HAL_MAX_DELAY);
    }
    HAL_FLASH_Lock();

    // 4. Verify CRC
    calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)APP_ADDRESS, fw_size / 4);
    if (calculated_crc != fw_crc) {
        HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
        return; // CRC mismatch
    }

    // 5. Wait for Reboot Command
    if (HAL_UART_Receive(&huart1, &received_cmd, 1, BOOTLOADER_TIMEOUT) != HAL_OK || received_cmd != OTA_CMD_REBOOT) {
         HAL_UART_Transmit(&huart1, (uint8_t*)nack_msg, 1, HAL_MAX_DELAY);
         return;
    }
    HAL_UART_Transmit(&huart1, (uint8_t*)ack_msg, 1, HAL_MAX_DELAY);
    HAL_Delay(100); // Allow time for ACK to be sent
    NVIC_SystemReset(); // Reboot
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_CRC_Init();

  /* USER CODE BEGIN 2 */
  // Wait for a moment to see if an OTA command comes in
  if (HAL_UART_Receive(&huart1, uart_rx_buffer, 1, BOOTLOADER_TIMEOUT) == HAL_OK) {
      if(uart_rx_buffer[0] == OTA_CMD_START) {
          // OTA command received, start update process
          handle_ota_update();
      }
  }

  // If no OTA command, or if OTA failed and returned, try to jump to app
  if (is_app_valid()) {
      jump_to_application();
  }

  // If no valid app and no OTA, stay in bootloader and blink LED
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	  HAL_Delay(250); // Blink faster to indicate bootloader mode
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
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
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  * where the assert_param error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */
