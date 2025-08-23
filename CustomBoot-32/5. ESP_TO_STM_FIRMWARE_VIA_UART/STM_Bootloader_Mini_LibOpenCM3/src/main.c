#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/cortex.h>
#include <string.h>

#define APP_ADDRESS (uint32_t)0x08004000
uint32_t pinState;
#define RX_CHUNK_SIZE 1024
uint8_t rx_buffer[RX_CHUNK_SIZE];
#define CHUNK_SIZE 520
#define huart1 "STM"

void usart_setup(void);
void JumpToAddress(uint32_t addr);
void goto_app(uint32_t addr);
void EraseUserApplication(uint32_t addr);
void WriteUserApplication(uint32_t addr, uint32_t *data, uint32_t dataSize, uint32_t offset);
void uart_transmit(uint32_t usart, const uint8_t *data, uint16_t len);
void uart_receive_blocking(uint32_t usart, uint8_t *buf, uint16_t len);
void ReceiveChunkOverUART(uint32_t addr, const char *str);

void usart_setup(void)
{

    // rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART1);
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO9);

    gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
                  GPIO_CNF_INPUT_FLOAT, GPIO10);

    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);
    usart_set_mode(USART1, USART_MODE_TX_RX);
    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
    usart_enable(USART1);
}

typedef void (*pFunction)(void);

void JumpToAddress(uint32_t addr)
{
    uint32_t jump_address = *(volatile uint32_t *)(addr + 4);

    /* Reset clocks to default reset state */
    RCC_APB2RSTR = 0xFFFFFFFF;
    RCC_APB2RSTR = 0x00000000;

    RCC_APB1RSTR = 0xFFFFFFFF;
    RCC_APB1RSTR = 0x00000000;

    RCC_APB2ENR = 0x00000000;
    RCC_APB1ENR = 0x00000000;
    RCC_AHBENR  = 0x00000014;  // SRAM + FLITF only

    RCC_CFGR = 0x00000000;

    /* Disable interrupts */
    cm_disable_interrupts();

    /* Disable SysTick */
    systick_counter_disable();
    systick_interrupt_disable();
    systick_clear();

    /* Set vector table to application start */
    SCB_VTOR = addr;
    cm_enable_interrupts();
    /* Set MSP and jump to app reset handler */
    __asm volatile (
        "msr msp, %[stack_ptr]  \n"
        "bx  %[reset_handler]   \n"
        :
        : [stack_ptr] "r" (*(volatile uint32_t *)addr),
          [reset_handler] "r" (jump_address)
        : 
    );
}



void goto_app(uint32_t addr)
{

    
    JumpToAddress(addr);
   
}

void EraseUserApplication(uint32_t addr)
{
    // HAL_StatusTypeDef success = HAL_ERROR;
    // uint32_t errorSector = 0;
    flash_unlock();
    for (uint32_t i = 0; i < 8; i++)
    {
        flash_erase_page(addr + i * (uint32_t)1024);
                       // uart_transmit(USART1, "1 kB erased\r", strlen("1 kB erased\r"));

    }
    flash_lock();
}

void WriteUserApplication(uint32_t addr, uint32_t *data, uint32_t dataSize, uint32_t offset)
{
    flash_unlock();
    for (uint32_t i = 0; i < dataSize; i++)
    {
        // HAL_StatusTypeDef success = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + offset + (i * 4), data[i]);
        flash_program_word(addr + offset + (i * 4), data[i]);
                     //   uart_transmit(USART1, "Wrote 512 kB\r", strlen("Wrote 512 kB\r"));

    }
    flash_lock();
}

void uart_transmit(uint32_t usart, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        usart_send_blocking(usart, data[i]);
    }
}

void uart_receive_blocking(uint32_t usart, uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        buf[i] = usart_recv_blocking(usart);
    }
}

void ReceiveChunkOverUART(uint32_t addr, const char *str)
{
 
    uart_transmit(USART1, str, strlen(str));
    uint32_t offset = 0;

    EraseUserApplication(addr);
    while (1)
    {
        memset(rx_buffer, 0, CHUNK_SIZE);
       
        uart_receive_blocking(USART1, rx_buffer, CHUNK_SIZE);
        
        if (rx_buffer[CHUNK_SIZE - 1] == 0xAF)
        {
            WriteUserApplication((uint32_t)addr, (uint32_t *)&rx_buffer[3], 128, offset);

            goto_app((uint32_t)addr);
        }
        else if (rx_buffer[CHUNK_SIZE - 1] == 0x00)
        {

            WriteUserApplication((uint32_t)addr, (uint32_t *)&rx_buffer[3], 128, offset);

            offset += 512;
            for (int i = 0; i < 5; i++)
    {
        gpio_clear(GPIOC, GPIO13); // LED ON
        for (int i = 0; i < 800; i++)
            __asm__("nop");
        gpio_set(GPIOC, GPIO13); // LED OFF
        for (int i = 0; i < 800; i++)
            __asm__("nop");
    }
                uart_transmit(USART1, "Ready\r", strlen("Ready\r"));

        }
    }
}

int main(void)
{
rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_GPIOB);

    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO0);
    usart_setup();
    for (int i = 0; i < 5; i++)
    {
        gpio_clear(GPIOC, GPIO13); // LED ON
        for (int i = 0; i < 800000; i++)
            __asm__("nop");
        gpio_set(GPIOC, GPIO13); // LED OFF
        for (int i = 0; i < 800000; i++)
            __asm__("nop");
    }
    while (1)
    {
        pinState = gpio_get(GPIOA, GPIO0);
        if (pinState == 0)
        {
            ReceiveChunkOverUART(APP_ADDRESS, "Send_A\r");
        }
        else
        {
            ReceiveChunkOverUART(APP_ADDRESS, "Send_B\r");
        }
    }
}