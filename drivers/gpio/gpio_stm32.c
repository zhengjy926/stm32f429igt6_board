/**
  ******************************************************************************
  * @file      : gpio_stm32.c
  * @author    : ZJY
  * @version   : V1.0
  * @date      : 2026-11-20
  * @brief     : STM32F4 GPIO 驱动，基于 LL 库
  * @attention : 1. EXTI 源映射依赖 SYSCFG 时钟，board_init() 里已使能；
  *              2. 驱动只负责 EXTI 线上的源映射/触发/使能，对应 IRQn 的
  *                 NVIC 使能、优先级和中断服务程序由板级代码负责；
  *              3. 输出速度固定为低速，需要高频翻转的引脚请在外设驱动里
  *                 自行改写 OSPEEDR；
  *              4. STM32F4 的 EXTI 只有边沿触发，电平触发返回 -ENOTSUP。
  ******************************************************************************
  */
#include "gpio.h"

#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_system.h"

/* Private types ------------------------------------------------------------*/

/** @brief 端口固有信息，放在 dev->config，由驱动私有 */
struct gpio_stm32_config {
    GPIO_TypeDef *port;   /**< GPIOA ~ GPIOK */
    uint32_t clock;       /**< LL_AHB1_GRP1_PERIPH_GPIOx */
    uint32_t exti_port;   /**< LL_SYSCFG_EXTI_PORTx，EXTI 源映射用 */
};

/* Private helpers ----------------------------------------------------------*/

/**
 * @brief  引脚号转单引脚掩码
 */
static uint32_t gpio_stm32_pin_mask(gpio_pin_t pin)
{
    return (uint32_t)1U << pin;
}

/**
 * @brief  把引脚号编码成 LL_SYSCFG_SetEXTISource() 需要的 Line 参数
 *
 * 编码格式：(0xF << ((pin % 4) * 4)) << 16 | (pin / 4)
 * 例如 pin = 7 -> 0xF0000001，表示 EXTICR[1] 的 bit[15:12]。
 */
static uint32_t gpio_stm32_exti_line(gpio_pin_t pin)
{
    uint32_t pos = ((uint32_t)pin & 0x3U) * 4U;

    return (((uint32_t)0xFU << pos) << 16U) | ((uint32_t)pin >> 2U);
}

/* Driver API ---------------------------------------------------------------*/

static int gpio_stm32_pin_configure(const struct device *dev, gpio_pin_t pin,
                                    gpio_flags_t flags)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;
    uint32_t mask;
    uint32_t mode;
    uint32_t pull;

    if (pin >= GPIO_PINS_PER_PORT) {
        return -EINVAL;
    }
    if ((flags & ~GPIO_FLAGS_VALID) != 0U) {
        return -EINVAL;
    }

    switch (flags & GPIO_PULL_MASK) {
    case GPIO_PULL_NONE:
        pull = LL_GPIO_PULL_NO;
        break;
    case GPIO_PULL_UP:
        pull = LL_GPIO_PULL_UP;
        break;
    case GPIO_PULL_DOWN:
        pull = LL_GPIO_PULL_DOWN;
        break;
    default:
        return -EINVAL; /* 0b11 是保留编码 */
    }

    mask = gpio_stm32_pin_mask(pin);

    if ((flags & GPIO_DIR_MASK) == GPIO_DIR_OUTPUT) {
        /* 先定电平再切模式，避免切换瞬间输出一个毛刺 */
        if ((flags & GPIO_INIT_MASK) != 0U) {
            LL_GPIO_SetOutputPin(cfg->port, mask);
        } else {
            LL_GPIO_ResetOutputPin(cfg->port, mask);
        }

        LL_GPIO_SetPinOutputType(cfg->port, mask,
                                 ((flags & GPIO_OTYPE_MASK) != 0U)
                                     ? LL_GPIO_OUTPUT_OPENDRAIN
                                     : LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinSpeed(cfg->port, mask, LL_GPIO_SPEED_FREQ_LOW);
        mode = LL_GPIO_MODE_OUTPUT;
    } else {
        mode = LL_GPIO_MODE_INPUT;
    }

    LL_GPIO_SetPinPull(cfg->port, mask, pull);
    LL_GPIO_SetPinMode(cfg->port, mask, mode);

    return 0;
}

static int gpio_stm32_pin_set(const struct device *dev, gpio_pin_t pin, uint32_t value)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;
    uint32_t mask;

    if (pin >= GPIO_PINS_PER_PORT) {
        return -EINVAL;
    }

    mask = gpio_stm32_pin_mask(pin);

    if (value != 0U) {
        LL_GPIO_SetOutputPin(cfg->port, mask);
    } else {
        LL_GPIO_ResetOutputPin(cfg->port, mask);
    }

    return 0;
}

static int gpio_stm32_pin_get(const struct device *dev, gpio_pin_t pin, uint32_t *value)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;
    uint32_t mask;

    if (pin >= GPIO_PINS_PER_PORT) {
        return -EINVAL;
    }

    mask = gpio_stm32_pin_mask(pin);
    *value = (LL_GPIO_IsInputPinSet(cfg->port, mask) != 0U) ? 1U : 0U;

    return 0;
}

static int gpio_stm32_pin_toggle(const struct device *dev, gpio_pin_t pin)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;

    if (pin >= GPIO_PINS_PER_PORT) {
        return -EINVAL;
    }

    LL_GPIO_TogglePin(cfg->port, gpio_stm32_pin_mask(pin));

    return 0;
}

static int gpio_stm32_pin_interrupt_configure(const struct device *dev, gpio_pin_t pin,
                                              gpio_int_mode_t mode)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;
    uint32_t line;

    if (pin >= GPIO_PINS_PER_PORT) {
        return -EINVAL;
    }

    /* 先把不支持的参数挡掉，避免误改已有配置 */
    switch (mode) {
    case GPIO_INT_NONE:
    case GPIO_INT_EDGE_RISING:
    case GPIO_INT_EDGE_FALLING:
    case GPIO_INT_EDGE_BOTH:
        break;
    case GPIO_INT_LEVEL_HIGH:
    case GPIO_INT_LEVEL_LOW:
        return -ENOTSUP; /* F4 EXTI 只支持边沿触发 */
    default:
        return -EINVAL;
    }

    line = gpio_stm32_pin_mask(pin);

    /* 先关线、清挂起，再改触发方式，避免改配置过程中产生伪中断 */
    LL_EXTI_DisableIT_0_31(line);
    LL_EXTI_DisableRisingTrig_0_31(line);
    LL_EXTI_DisableFallingTrig_0_31(line);
    LL_EXTI_ClearFlag_0_31(line);

    if (mode == GPIO_INT_NONE) {
        return 0;
    }

    LL_SYSCFG_SetEXTISource(cfg->exti_port, gpio_stm32_exti_line(pin));

    if ((mode == GPIO_INT_EDGE_RISING) || (mode == GPIO_INT_EDGE_BOTH)) {
        LL_EXTI_EnableRisingTrig_0_31(line);
    }
    if ((mode == GPIO_INT_EDGE_FALLING) || (mode == GPIO_INT_EDGE_BOTH)) {
        LL_EXTI_EnableFallingTrig_0_31(line);
    }

    LL_EXTI_EnableIT_0_31(line);

    return 0;
}

static int gpio_stm32_pin_irq_ack(const struct device *dev, gpio_pin_t pin)
{
    (void)dev;

    if (pin >= GPIO_PINS_PER_PORT) {
        return -EINVAL;
    }

    LL_EXTI_ClearFlag_0_31(gpio_stm32_pin_mask(pin));

    return 0;
}

static int gpio_stm32_port_get(const struct device *dev, gpio_mask_t *value)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;

    *value = (gpio_mask_t)(LL_GPIO_ReadInputPort(cfg->port) & 0xFFFFU);

    return 0;
}

static int gpio_stm32_port_set_bits(const struct device *dev, gpio_mask_t mask)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;

    LL_GPIO_SetOutputPin(cfg->port, (uint32_t)mask);

    return 0;
}

static int gpio_stm32_port_clear_bits(const struct device *dev, gpio_mask_t mask)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;

    LL_GPIO_ResetOutputPin(cfg->port, (uint32_t)mask);

    return 0;
}

static int gpio_stm32_port_toggle_bits(const struct device *dev, gpio_mask_t mask)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;

    LL_GPIO_TogglePin(cfg->port, (uint32_t)mask);

    return 0;
}

static int gpio_stm32_port_irq_status(const struct device *dev, gpio_mask_t *pending)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;
    uint32_t owned = 0U;
    uint32_t pin;

    /*
     * EXTI 线 n 全局只有一条，当前映射到哪个端口要看 SYSCFG->EXTICR。
     * 只报告映射到本端口的线，否则 gpioa/gpiob 会同时报同一批挂起位。
     */
    for (pin = 0U; pin < (uint32_t)GPIO_PINS_PER_PORT; pin++) {
        if (LL_SYSCFG_GetEXTISource(gpio_stm32_exti_line((gpio_pin_t)pin)) == cfg->exti_port) {
            owned |= (uint32_t)1U << pin;
        }
    }

    *pending = (gpio_mask_t)(LL_EXTI_ReadFlag_0_31(0x0000FFFFU) & owned);

    return 0;
}

/* Device registration ------------------------------------------------------*/

static int gpio_stm32_init(const struct device *dev)
{
    const struct gpio_stm32_config *cfg = (const struct gpio_stm32_config *)dev->config;

    LL_AHB1_GRP1_EnableClock(cfg->clock);

    return 0;
}

static const struct gpio_driver_api gpio_stm32_api = {
    .pin_configure = gpio_stm32_pin_configure,
    .pin_set = gpio_stm32_pin_set,
    .pin_get = gpio_stm32_pin_get,
    .pin_toggle = gpio_stm32_pin_toggle,
    .pin_interrupt_configure = gpio_stm32_pin_interrupt_configure,
    .pin_irq_ack = gpio_stm32_pin_irq_ack,
    .port_get = gpio_stm32_port_get,
    .port_set_bits = gpio_stm32_port_set_bits,
    .port_clear_bits = gpio_stm32_port_clear_bits,
    .port_toggle_bits = gpio_stm32_port_toggle_bits,
    .port_irq_status = gpio_stm32_port_irq_status,
};

/* GPIO_STM32_DEVICE 的最后一个展开项不带分号，由调用处的分号收尾 */
#define GPIO_STM32_DEVICE(id, name, port_, clock_, exti_port_)  \
    static const struct gpio_stm32_config id##_cfg = {          \
        .port = (port_),                                        \
        .clock = (clock_),                                      \
        .exti_port = (exti_port_),                              \
    };                                                          \
    DEVICE_DEFINE(id, name, &id##_cfg, &gpio_stm32_api, NULL,   \
                  gpio_stm32_init, NULL)

GPIO_STM32_DEVICE(gpioa, "GPIOA", GPIOA, LL_AHB1_GRP1_PERIPH_GPIOA, LL_SYSCFG_EXTI_PORTA);
GPIO_STM32_DEVICE(gpiob, "GPIOB", GPIOB, LL_AHB1_GRP1_PERIPH_GPIOB, LL_SYSCFG_EXTI_PORTB);
GPIO_STM32_DEVICE(gpioc, "GPIOC", GPIOC, LL_AHB1_GRP1_PERIPH_GPIOC, LL_SYSCFG_EXTI_PORTC);
GPIO_STM32_DEVICE(gpiod, "GPIOD", GPIOD, LL_AHB1_GRP1_PERIPH_GPIOD, LL_SYSCFG_EXTI_PORTD);
GPIO_STM32_DEVICE(gpioe, "GPIOE", GPIOE, LL_AHB1_GRP1_PERIPH_GPIOE, LL_SYSCFG_EXTI_PORTE);
GPIO_STM32_DEVICE(gpiof, "GPIOF", GPIOF, LL_AHB1_GRP1_PERIPH_GPIOF, LL_SYSCFG_EXTI_PORTF);
GPIO_STM32_DEVICE(gpiog, "GPIOG", GPIOG, LL_AHB1_GRP1_PERIPH_GPIOG, LL_SYSCFG_EXTI_PORTG);
GPIO_STM32_DEVICE(gpioh, "GPIOH", GPIOH, LL_AHB1_GRP1_PERIPH_GPIOH, LL_SYSCFG_EXTI_PORTH);
GPIO_STM32_DEVICE(gpioi, "GPIOI", GPIOI, LL_AHB1_GRP1_PERIPH_GPIOI, LL_SYSCFG_EXTI_PORTI);
GPIO_STM32_DEVICE(gpioj, "GPIOJ", GPIOJ, LL_AHB1_GRP1_PERIPH_GPIOJ, LL_SYSCFG_EXTI_PORTJ);
GPIO_STM32_DEVICE(gpiok, "GPIOK", GPIOK, LL_AHB1_GRP1_PERIPH_GPIOK, LL_SYSCFG_EXTI_PORTK);
