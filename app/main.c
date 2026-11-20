/**
  ******************************************************************************
  * @file      : main.c
  * @author    : ZJY
  * @version   : V1.0
  * @date      : 2026-11-20
  * @brief     : 板级自检入口
  ******************************************************************************
  */
#include "board.h"

#include "gpio.h"
#include "gpio/gpio_stm32.h"
#include "stm32f4xx_ll_utils.h"

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  初始化失败的停靠点：死循环，便于调试器查看现场
 */
static void panic_loop(void)
{
    while (1) {
    }
}

/**
 * @brief  PB0 每 500ms 翻转一次的自检
 * @note   依赖 board_init() 已经跑完（GPIOB 时钟已由 device_init 打开、
 *         SysTick 已由 SystemClock_Config 配成 1ms）。
 *         本函数不返回，只用于板子点灯/示波器验证。
 */
static void gpio_test_pb0(void)
{
    const struct device *dev = DEVICE_GET(gpiob);

    if (gpio_pin_configure(dev, 0U, GPIO_DIR_OUTPUT | GPIO_PUSH_PULL | GPIO_INIT_LOW) < 0) {
        panic_loop();
    }

    while (1) {
        (void)gpio_pin_toggle(dev, 0U);
        LL_mDelay(500U);
    }
}

/* Exported functions --------------------------------------------------------*/

int main(void)
{
    if (board_init() < 0) {
        panic_loop();
    }

    gpio_test_pb0();

    return 0;
}
