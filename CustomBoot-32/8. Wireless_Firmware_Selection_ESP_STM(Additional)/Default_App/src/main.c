#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>


int main(void)
{
    SCB_VTOR = 0x08001800;
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    while (1)
    {
        gpio_clear(GPIOC, GPIO13); // LED ON
        for (int i = 0; i < 800000; i++)
            __asm__("nop");
        gpio_set(GPIOC, GPIO13); // LED OFF
        for (int i = 0; i < 800000; i++)
            __asm__("nop");
    }
}