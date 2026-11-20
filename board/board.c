#include <board.h>

#include "device.h"
#include "err_def.h"
#include "gpio/gpio_stm32.h"

#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"

void SystemClock_Config(void);

/* Private variables ---------------------------------------------------------*/

/*
 * 板级设备表：board_init() 按顺序 init 这些设备。
 * 目前只有 GPIO，device_init() 里会打开对应端口的 AHB1 时钟；
 * 用不到的端口可以从表里删掉，省一路时钟。
 */
static const struct device *const board_devices[] = {
    DEVICE_GET(gpioa),
    DEVICE_GET(gpiob),
    DEVICE_GET(gpioc),
    DEVICE_GET(gpiod),
    DEVICE_GET(gpioe),
    DEVICE_GET(gpiof),
    DEVICE_GET(gpiog),
    DEVICE_GET(gpioh),
    DEVICE_GET(gpioi),
    DEVICE_GET(gpioj),
    DEVICE_GET(gpiok),
};

#define BOARD_DEVICE_COUNT (sizeof(board_devices) / sizeof(board_devices[0]))

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  依次初始化板级设备
 * @retval 0        成功（重复调用 board_init() 时也算成功）
 * @retval 负 errno 某个设备初始化失败，返回该错误
 */
static int board_devices_init(void)
{
    uint32_t i;
    int rc;

    for (i = 0U; i < BOARD_DEVICE_COUNT; i++) {
        rc = device_init(board_devices[i]);
        if (rc == -EALREADY) {
            continue;
        }
        if (rc < 0) {
            return rc;
        }
    }

    return 0;
}

int board_init(void)
{
    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    /* System interrupt init*/
    NVIC_SetPriorityGrouping(3);

    /* SysTick_IRQn interrupt configuration */
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),15, 0));

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* 板级设备初始化：GPIO 端口时钟等 */
    return board_devices_init();
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);
    while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_5)
    {
    }
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_PWR_EnableOverDriveMode();
    LL_RCC_HSE_Enable();

    /* Wait till HSE is ready */
    while(LL_RCC_HSE_IsReady() != 1)
    {
    }
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_15, 216, LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_Enable();

    /* Wait till PLL is ready */
    while(LL_RCC_PLL_IsReady() != 1)
    {
    }
    while (LL_PWR_IsActiveFlag_VOS() == 0)
    {
    }
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

    /* Wait till System clock is ready */
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
    {
    }
    LL_Init1msTick(180000000);
    LL_SetSystemCoreClock(180000000);
    LL_RCC_SetTIMPrescaler(LL_RCC_TIM_PRESCALER_TWICE);
}
